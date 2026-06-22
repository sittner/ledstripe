import os, sys
from PIL import Image

CHARS = 256
SIZE_X = 12
SIZE_Y = 16


im = Image.open("12x16.bmp")

pix = im.load()

print("const uint16_t font[" + str(CHARS) + "][" + str(SIZE_X) + "] = {")

for c in range(0, CHARS):
  os_x = (c & 0x0f) * SIZE_X
  os_y = (c >> 4) * SIZE_Y

  line = "  { "
  for x in range(0, SIZE_X):
    val = 0
    for y in range(0, SIZE_Y):
      if (pix[os_x + x, os_y + y] == 0):
        val |= (1 << y)
    if (x > 0):
      line = line + ", "
    line = line + "0x" + format(val, '04x');
  line = line + " },"
  print(line)

print("};")

