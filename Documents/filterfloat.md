# filterfloat — Spatial Low-Pass / High-Pass Filter for Float Images

## Purpose

Applies a separable rectangular (box) filter to a big-endian float32 image,
skipping pixels whose value falls at or below a no-data threshold (`minValue`).
The filter can produce a smoothed (low-pass) image, or be inverted to a
high-pass residual. A second iteration converts the box kernel into a
triangular (piecewise-linear) approximation of a Gaussian.

Input can be a raw binary stream (stdin) or a georeferenced VRT; output can be
binary stdout or a GeoTIFF + VRT sidecar.

---

## Usage

```
# Binary I/O (default — MSB float32)
filterfloat [opts] < input.bin > output.bin

# Georeferenced I/O
filterfloat -inputVRT in.vrt -tiff out.vrt [opts]
```

### Options

| Option                | Default    | Description |
|-----------------------|------------|-------------|
| `-nr <n>`             | 1248       | Range (column) dimension in pixels (binary mode only) |
| `-na <n>`             | 1318       | Azimuth (row) dimension in pixels (binary mode only) |
| `-w <n>`              | 50         | Filter half-width in both directions |
| `-wr <n>`             | 50         | Filter half-width in range (columns) |
| `-wa <n>`             | 50         | Filter half-width in azimuth (rows) |
| `-nIterations <n>`    | 1          | Number of filtering passes |
| `-minValue <v>`       | −2×10⁹     | Pixels ≤ this value are treated as no-data and excluded from averaging |
| `-hiPass`             | (off)      | High-pass: output ← original − low-pass |
| `-inputVRT <file>`    | (none)     | Read from VRT; auto-detects nr/na |
| `-tiff <file>`        | (none)     | Write GeoTIFF + VRT instead of binary stdout |
| `-LSB`                | (off)      | Use LSB byte order for binary I/O (default is MSB) |

### Output Metadata (VRT mode)

When `-tiff` is used, processing parameters are stored as dataset metadata
keys so the provenance travels with the file:

| Key                       | Value |
|---------------------------|-------|
| `filterfloat_wr`          | Range filter width |
| `filterfloat_wa`          | Azimuth filter width |
| `filterfloat_type`        | `lopass` or `hipass` |
| `filterfloat_nIterations` | Number of passes |
| `filterfloat_minValue`    | No-data threshold |

The band `noDataValue` in both the GeoTIFF and VRT is set to `minValue`.

---

## Algorithm

### 1. Mask initialisation

A boolean mask of the same dimensions as the image is built before filtering:

```
mask[i][j] = 1   if image[i][j] > minValue   (valid)
mask[i][j] = 0   otherwise                    (no-data hole)
```

No-data pixels are excluded from sums and counts throughout. After the filter
pass the mask is used to reset any pixel that was originally invalid back to
`minValue`, so holes are neither filled nor smeared — they remain as gaps in
the output.

### 2. High-pass save

If `-hiPass` is requested, a copy of the original image is saved before
filtering. After the low-pass step the residual is computed as:

```
output[i][j] = original[i][j] - lowpass[i][j]
```

No-data pixels in the original are preserved as-is in the residual (they will
be reset to `minValue` by the mask step above).

### 3. Separable sliding-window box filter

The rectangular filter is decomposed into two independent 1-D passes:

1. **Range pass (columns):** each row is filtered along its column axis into a
   temporary image `tmpImage`.
2. **Azimuth pass (rows):** the temporary image is filtered along the row axis,
   writing back to `inputImage->phase`.

Each 1-D pass uses an **O(1)-per-pixel sliding window** (a running
sum with an associated running count of valid pixels). For a kernel of
half-width *h* and current position *j*, the running sum is updated as:

```
sum[j] = sum[j-1]  +  in[j+h] * mask[j+h]
                   -  in[j-h-1] * mask[j-h-1]
count[j] = count[j-1] + mask[j+h] - mask[j-h-1]
output[j] = sum[j] / count[j]   (if count > 0)
```

The boundary regions at the start and end of each line are handled separately
with a growing/shrinking window that adds two pixels per step as the kernel
slides on from the edge. This mirrors the standard separable box-filter
boundary treatment while maintaining exact mask accounting.

A second mask (`mask1`) is derived from the range pass: a pixel's `mask1`
entry is 1 only if at least one valid source pixel contributed to its range
average. This prevents the azimuth pass from propagating averages of averages
outward beyond the original valid data footprint.

### 4. Kernel shape and iterations

| `nIterations` | Effective kernel |
|---------------|-----------------|
| 1             | Rectangular (box) — sharp edges in frequency domain |
| 2             | Triangular — equivalent to convolving two boxes; much closer to Gaussian |
| *n*           | B-spline of order *n* — approaches Gaussian as *n* → ∞ |

The filter widths are forced to be odd (incremented by 1 if even) so the kernel
has a unique central pixel.

---

## Dependencies

- `filterFloat.h` / `filterFloatImage.c` — core filter implementation
- `intFloat/getPhaseImage.c` — binary image input (reads from a file or stdin)
- `intFloat/vrtIO.c` — VRT / GeoTIFF I/O (`-inputVRT`, `-tiff`)
- `clib/standard.c` — `freadBS` / `fwriteBS` for MSB byte-swap I/O
- `mosaicSource/common/common.h` — `unwrapPhaseImage` struct
- GDAL (only linked when using VRT I/O)

---

## Notes

- `filterfloat` does **not** fill holes — it only averages valid pixels
  within the kernel window. Use `intfloat` to fill holes before filtering if
  seamless coverage is needed.
- The filter is data-adaptive near edges and holes: the output value at a
  pixel is the mean of however many valid neighbours fall within the kernel
  window, not the mean over the full kernel. This prevents no-data from
  leaking into valid regions.
- Binary mode always reads from stdin (even when `-nr`/`-na` are specified
  the image is still read from stdin, not a named file). The VRT path
  (`-inputVRT`) is the only mode that reads a named input file.
