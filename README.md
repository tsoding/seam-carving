# Seam Carving

Content Aware Resizing using [Seam Carving](https://en.m.wikipedia.org/wiki/Seam_carving)

![Lena_512](./images/Lena_512.png)
![Lena_162](./images/Lena_162.png)

## Quick Start

```console
$ cc -o nob nob.c
$ ./nob ./images/Lena_512.png output.png
$ feh output.png
```

After compressing the image horizontally, we can flip it and apply the same algorithm to compress it vertically as well:

```console
$ convert output.png -rotate 90 output.png
$ ./nob output.png output_two_sided.png
$ convert output.png -rotate -90 output.png
$ convert output_two_sided.png -rotate -90 output_two_sided.png
$ feh output_two_sided.png
```
