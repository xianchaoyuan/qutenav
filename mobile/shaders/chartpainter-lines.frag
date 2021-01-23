#version 310 es

precision mediump float;

const uint SOLID_PATT = 0x3ffffU;
const uint PAT_LEN = 18U;
uniform uint pattern; // an integer representing the bitwise pattern
uniform vec4 base_color;

in float texCoord;
out vec4 color;

void main(void) {

  if (pattern == SOLID_PATT) {
    color = base_color;
  } else {
    // create a filter with period patlen.
    uint bitpos = uint(round(texCoord)) % PAT_LEN;
    uint bit = (1U << bitpos);
    // discard the bit if it doesn't match the masking pattern
    if ((pattern & bit) == 0U) {
      color = vec4(base_color.rgb, 0.);
    } else {
      color = base_color;
    }
  }
}
