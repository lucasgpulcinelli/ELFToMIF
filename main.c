#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HEADER_STR                                                             \
  "-- Codigo gerado pelo montador\n"                                           \
  "WIDTH=16;\n"                                                                \
  "DEPTH=32768;\n"                                                             \
  "ADDRESS_RADIX=UNS;\n"                                                       \
  "DATA_RADIX=BIN;\n"                                                          \
  "CONTENT BEGIN\n"

#define END_STR "END;\n"

void getELFTextData(int fd, char *data) {
  elf_version(EV_CURRENT);

  Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
  if (elf == NULL) {
    fprintf(stderr, "error: %s\n", elf_errmsg(0));
    exit(-1);
  }

  size_t str_index_off;
  elf_getshdrstrndx(elf, &str_index_off);

  Elf32_Shdr *section_32;
  Elf_Scn *section = NULL;
  while ((section = elf_nextscn(elf, section)) != NULL) {
    section_32 = elf32_getshdr(section);

    char *name = elf_strptr(elf, str_index_off, section_32->sh_name);

    if (strcmp(name, ".text") == 0) {
      break;
    }
  }

  if (section == NULL) {
    fprintf(stderr, "error: ELF does not contain a .text section\n");
    exit(-1);
  }

  size_t elf_data_total_size = 0;
  Elf_Data *elf_data = NULL;
  while ((elf_data = elf_getdata(section, elf_data)) != NULL) {
    elf_data_total_size += elf_data->d_size;
    if (elf_data_total_size > (1 << 16)) {
      fprintf(stderr, "error: ELF bigger than max MIF size\n");
    }
    memcpy(data, elf_data->d_buf, elf_data->d_size);
  }

  elf_end(elf);
}

void char2Bin(char dest[9], char in) {
  for (int i = 0; i < 8; i++) {
    dest[7 - i] = '0' + ((in >> i) & 1);
  }

  dest[8] = '\0';
}

void printMIFLine(FILE *fptr, int line, char b1, char b2) {
  char b1_str[9];
  char b2_str[9];
  char2Bin(b1_str, b1);
  char2Bin(b2_str, b2);

  fprintf(fptr, "%d:%s%s;\n", line, b1_str, b2_str);
}

void writeMIFToFile(FILE *fptr, char *data) {
  fprintf(fptr, HEADER_STR);

  for (int i = 0; i < (1 << 15); i++) {
    printMIFLine(fptr, i, data[2 * i], data[2 * i + 1]);
  }

  fprintf(fptr, END_STR);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: \"%s <input> <output>.mif\"\n", argv[0]);
    exit(-1);
  }

  int fin = open(argv[1], O_RDONLY);
  if (fin < 0) {
    fprintf(stderr, "error: %s: %s\n", argv[1], strerror(errno));
    exit(-1);
  }
  char *data = calloc((1 << 16), sizeof(char));

  getELFTextData(fin, data);

  close(fin);

  FILE *fout = fopen(argv[2], "w");
  if (fout == NULL) {
    fprintf(stderr, "error: %s: %s\n", argv[2], strerror(errno));
    exit(-1);
  }

  writeMIFToFile(fout, data);

  fclose(fout);
  free(data);
}
