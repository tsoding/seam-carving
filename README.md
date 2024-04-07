# Seam Carving

Content Aware Resizing using [Seam Carving](https://en.m.wikipedia.org/wiki/Seam_carving)

![Lena_512](./Lena_512.png)
![Lena_162](./Lena_162.png)

## Usage

Compile the program with  :
```console
$ cc -o nob nob.c
```
Once compiled, you can use the `nob` executable like this:
```
USAGE
  ./nob file_path

  file_path      the path to your image file

OPTIONS
  -h, --help     Display this message and exit.
```

**Example** :
```console
$ ./nob Broadway_tower_edit.jpg
```
Then you'll have the result saved as an ```output.png``` file :)