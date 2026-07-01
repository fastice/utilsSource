# intfloat — Hole-Filling Interpolator for Float Images

## Purpose

Replaces pixels whose value falls at or below a no-data threshold (`minValue`)
with values interpolated from surrounding valid pixels. Three interpolation
methods are available: inverse-distance weighting, plane (linear), and
biquadratic surface fitting. After filling, isolated islands of valid data that
may be artefacts can be culled by area or by bounding-box width/height.

Input can be a raw binary file or stdin (MSB big-endian float32) or a
georeferenced VRT; output can be binary stdout or a GeoTIFF + VRT sidecar.

---

## Usage

```
# Binary I/O (default — MSB float32)
intfloat [opts] imageFile

# Georeferenced I/O
intfloat -inputVRT in.vrt -tiff out.vrt [opts]
```

### Options

| Option                     | Default    | Description |
|----------------------------|------------|-------------|
| `-nr <n>`                  | 1248       | Range (column) dimension (binary mode only) |
| `-na <n>`                  | 1318       | Azimuth (row) dimension (binary mode only) |
| `-minValue <v>`            | −2×10⁹     | Pixels ≤ this value are treated as holes |
| `-interpLength <n>`        | 50         | Search radius in pixels — only border points within this distance of a hole pixel are used |
| `-thresh <n>`              | 50000      | Holes with more pixels than this are skipped |
| `-ratThresh <f>`           | 0.2        | Compact-shape rejection ratio: holes rejected when `nH/(bbox_width × bbox_height) > ratThresh` and `nH > interpLength²` (see [screenHoles](#screenHoles)) |
| `-linear`                  | (default)  | Plane fit via SVD (3 coefficients: constant + x + y) |
| `-quadratic`               | (off)      | Biquadratic surface fit via SVD (6 coefficients) |
| `-wdist`                   | (off)      | Inverse-distance-squared weighting |
| `-fast`                    | (off)      | Reduce point-search density — acceptable for large smooth fields |
| `-allowBreaks`             | (off)      | Allow filling holes that span the full image width or height |
| `-padEdges`                | (off)      | Do not mask border strips — allow filling right to the image edge |
| `-islandThresh <n>`        | (off)      | After filling, cull valid-data islands whose bounding box is narrower than `n` pixels in either direction (uses `cullIslands` from `speckleSource`) |
| `-islandAreaThresh <n>`    | (off)      | After filling, cull valid-data islands with fewer than `n` pixels |
| `-inputVRT <file>`         | (none)     | Read from VRT; auto-detects nr/na |
| `-tiff <file>`             | (none)     | Write GeoTIFF + VRT instead of binary stdout |
| `-LSB`                     | (off)      | Use LSB byte order for binary I/O (default is MSB) |

### Output Metadata (VRT mode)

When `-tiff` is used, processing parameters are stored as dataset metadata:

| Key                    | Value |
|------------------------|-------|
| `intfloat_interpLength`| Search radius used |
| `intfloat_interpType`  | `linear`, `quadratic`, or `wdist` |
| `intfloat_minValue`    | No-data threshold |
| `intfloat_thresh`      | Maximum hole size that was filled |

---

## Hole-filling Algorithm

### Overview

The algorithm proceeds in six logical stages:

```
1. Build mask            — classify every pixel as valid or hole
2. Clip border strips    — prevent extrapolation beyond the data footprint
3. Connected-components  — label each contiguous hole region uniquely
4. Characterise holes    — record pixel coordinates and adjacent border points
5. Screen holes          — skip holes that are too large, too compact, or image-spanning
6. Interpolate           — fit a surface to border points; evaluate at hole pixels
```

After interpolation, optional island culling removes isolated valid-data patches
that may have been exposed or created by the fill step.

---

### Stage 1 — Mask initialisation

A `char` mask image is built with the same dimensions as the input:

```
mask[i][j] = VALIDPIXEL             if image[i][j] > minValue
mask[i][j] = VALIDPIXEL | REGION    if image[i][j] <= minValue  (hole)
```

The `REGION` bit marks the pixel as belonging to a hole. The `VALIDPIXEL` base
value is required for compatibility with the connected-components labeller
(`labelRegions`) which was inherited from the phase-unwrapping code and uses
the same bit-field convention.

---

### Stage 2 — Border clipping (`clipBorders`)

Unless `-padEdges` is specified, the outermost strip of no-data rows and columns
is excluded from filling. The function scans inward from each of the four edges
until it finds the first row (or column) that contains at least one valid pixel:

```
top edge:    scan rows 0, 1, … until a row has a valid pixel → mask those rows invalid
bottom edge: scan rows na-1, na-2, … similarly
left edge:   scan columns 0, 1, … similarly
right edge:  scan columns nr-1, nr-2, … similarly
```

This prevents the algorithm from attempting to extrapolate data beyond the
real coverage boundary. The effective inner image dimensions `width × height`
are recorded for the break-detection test in Stage 5.

---

### Stage 3 — Connected-components labelling

The masked image is passed to `labelRegions` (from `unwrapSource`), which
assigns a unique integer label to each contiguous group of hole pixels
(`REGION`-flagged pixels). Each such group is called a *hole*. The labels
are stored in a `labels[na][nr]` integer array:

```
labels[i][j] = k     if pixel (i,j) is part of hole k
labels[i][j] = LARGEINT   if pixel (i,j) is valid
```

---

### Stage 4 — Hole characterisation (three passes)

After labelling, the algorithm makes three passes over the image to build a
`hole` structure for each labeled region:

```c
typedef struct {
    int nH;          /* number of pixels inside the hole */
    int nB;          /* number of valid border pixels adjacent to the hole */
    int *xh, *yh;    /* coordinates of all hole pixels */
    int *xb, *yb;    /* coordinates of all border pixels */
} hole;
```

**Pass 1 — find maximum label.**
Scan the label array once to find `maxLabel`, the highest label value.
Allocate an array of `hole` structures indexed 1…maxLabel.

**Pass 2 — count holes and borders.**
For each pixel:
- If it is a hole pixel (has a finite label), increment `holes[label].nH`.
- If it is a valid pixel adjacent to one or more hole pixels, mark it as a
  `BORDER` pixel and increment `holes[label].nB` for each adjacent hole label.

The adjacency search for border pixels uses a `(2*nd+1) × (2*nd+1)` neighbourhood
centred on the valid pixel, where `nd = 2` by default (`nd = 1` with `-fast`).

After Pass 2 the `nH` and `nB` counts are known, so coordinate arrays are
allocated.

**Pass 3 — save coordinates.**
Repeat the same scan, this time storing the actual `(x, y)` coordinates of
hole pixels and border pixels into the preallocated arrays.

---

### Stage 5 — Hole screening (`screenHoles`) {#screenHoles}

Each hole is tested against three rejection criteria. Any hole that fails a
test has its `nH` reset to zero, which causes it to be skipped in Stage 6.

**1. Size threshold (`-thresh`).**
Holes with more than `thresh` pixels are too large to interpolate reliably:

```
if nH >= thresh → reject
```

**2. Image-spanning break check.**
A hole that spans the full width or height of the valid data region effectively
cuts the image in two halves, meaning valid border pixels exist on only one side.
Fitting across such a break would extrapolate rather than interpolate:

```
bbox_width  = maxX − minX + 1
bbox_height = maxY − minY + 1
if bbox_width  >= image_width  → reject (horizontal break)
if bbox_height >= image_height → reject (vertical break)
```

This check is suppressed by `-allowBreaks`.

**3. Compactness / shape ratio (`-ratThresh`).**
A hole that is roughly isodiametric (blob-shaped) is unlikely to be reliably
filled: the border points encircle it and the plane/quadratic fit degenerates.
Long, narrow features (rivers, fjord outlines, scan-line stripes) do pass this
test because their bounding box is much larger than their area. The test is:

```
ratio = nH / (bbox_width × bbox_height)
if nH > interpLength² AND ratio > ratThresh → reject
```

The `interpLength²` guard prevents rejection of very small compact holes where
the ratio is high simply because the bounding box is tiny.

---

### Stage 6 — Interpolation

For each non-rejected hole (indexed `i`), the algorithm iterates over the
`nH` hole pixels in raster order:

#### Point-search frequency

For large holes (nH > 100) a border-point search is not run at every hole
pixel. Instead, the search is only rerun when the current pixel has moved
more than `skip` pixels from the previous search site:

```
skip = 5   (default)
skip = 7   (-fast mode)
```

Between search updates the previously computed surface coefficients are reused
to evaluate the interpolant (`update = FALSE`). The search is also forced when
the nearest border point is closer than `skip` pixels (i.e., near the hole
edge where the geometry is changing rapidly).

#### Border-point search (`findPoints`)

For the current hole pixel at `(x, y)`, every border pixel `(xb[k], yb[k])`
is tested:

```
d² = (x - xb[k])² + (y - yb[k])²
if d² < interpLength² → include this border point
```

Points within `interpLength` pixels are collected into `data[]` (coordinates),
`zTmp[]` (image values), and `dTmp[]` (distances). The minimum distance to any
border point is also recorded as `minD` and used for the skip-update logic.

At least 4 border points are required to attempt interpolation (`nPts > 3`).

#### Interpolation methods

**Inverse-distance weighting (`-wdist`):**

Each border point contributes with weight $w_k = 1/d_k^2$:

$$
z = \frac{\sum_k w_k \, z_k}{\sum_k w_k}
$$

Also used as a fallback for `-quadratic` when fewer than 6 points are available
(SVD with 6 unknowns requires at least 6 observations).

**Linear / plane fit (`-linear`, default):**

Fits the model $z = a_1 + a_2 x + a_3 y$ to the border points by singular value
decomposition (SVD via `svdfit` from `cRecipes`). The three coefficients are
solved in a weighted least-squares sense (unit weights, $\sigma_k = d_k$).
The fit is only recomputed on `update = TRUE` frames; between updates the stored
coefficients are re-evaluated at the new `(x, y)` position.

**Quadratic fit (`-quadratic`):**

Fits the model $z = a_1 + a_2 x + a_3 y + a_4 xy + a_5 x^2 + a_6 y^2$ (6
coefficients) by the same SVD mechanism. Requires at least 6 border points.

---

### Island culling (post-processing)

After filling, two optional passes remove spurious or isolated valid-data
regions:

**`-islandAreaThresh n`** — `clipIslands` runs a fresh connected-components
labelling on the *valid* pixels (inverted mask: `REGION` marks valid data).
Any connected region with fewer than `n` valid pixels is reset to `minValue`.

**`-islandThresh n`** — `cullIslands` (from `speckleSource/Cullst`) scans
each labeled region and removes any whose bounding box in either direction is
narrower than `n` pixels.

---

## Key Data Structures

```c
/* One per contiguous hole region */
typedef struct {
    int label;
    int nH, cH;          /* total/counted hole pixels */
    int nB, cB;          /* total/counted border pixels */
    int *xh, *yh;        /* hole pixel coordinates  [nH] */
    int *xb, *yb;        /* border pixel coordinates [nB] */
} hole;

/* The image container shared with filterfloat */
typedef struct {
    int rangeSize, azimuthSize;
    unsigned char byteOrder;
    float **phase;       /* 2-D row-pointer array [az][range] */
} unwrapPhaseImage;
```

---

## Dependencies

- `intFloat.h` / `intFloatImage.c` — core interpolation and island-clip logic
- `intfloat/getPhaseImage.c` — binary image input
- `intfloat/vrtIO.c` — VRT / GeoTIFF I/O
- `clib/standard.c` — `freadBS` / `fwriteBS` MSB byte-swap I/O
- `unwrapSource/unWrap/unwrap.h` — `labelRegions` connected-components labeller
- `speckleSource/Cullst/cullst.h` — `cullIslands` bounding-box island filter
- `cRecipes/cRecipes.h` — `svdfit` for plane and quadratic surface fitting
- `mosaicSource/common/common.h` — shared constants and `unwrapImageStructure`
- GDAL (only linked when using VRT I/O)

---

## Typical Usage in the GrIMP Pipeline

`intfloat` is called by `nisargrimpworkflow.ROFFtoGrimp` and
`mosaicworkflow.setupquarters` to fill gaps in velocity offset maps before
mosaicking. Common call pattern:

```bash
intfloat -interpLength 100 -thresh 20000 -islandAreaThresh 200 \
         -inputVRT offsets.vrt -tiff offsets_filled.vrt
```

The `-thresh` limit prevents bridging large ocean gaps; `-islandAreaThresh`
removes isolated noise patches that survive the initial culling step.

---

## Notes

- The connected-components labeller was inherited from the phase-unwrapping code
  (`labelRegions`). It uses the same `VALIDPIXEL` / `REGION` / `BORDER` bit-flag
  conventions; the `bCuts` field of the unwrap image structure is pointed at
  `intfloat`'s local mask array.
- For the `-linear` and `-quadratic` fits, coefficients are updated only when the
  hole pixel has moved more than `skip` pixels from the last update site. This
  amortises the SVD cost over several adjacent pixels whose surrounding border
  geometry is nearly identical. The approximation error is negligible for smooth
  velocity or phase fields over typical hole sizes.
- `interpLength` controls both how far away border points are searched and,
  implicitly via the `interpLength²` guard in `screenHoles`, which compact holes
  are considered too large to fill reliably.
