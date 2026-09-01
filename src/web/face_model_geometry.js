// AUTO-GENERATED from src/core/math_defs.hpp by tools/generate_web_geometry.py
// DO NOT EDIT MANUALLY. Single Source of Truth is src/core/face_model_geometry.hpp / math_defs.hpp.

(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.FaceModelGeometry = factory();
  }
}(typeof self !== 'undefined' ? self : this, function () {
  return {
    DEFAULT_HFOV_DEG: 65.0,
    DEFAULT_FOCAL_TAN_HALF: 0.6370702608, // tan(65 deg / 2)
    calculateDefaultFocalLength: function (width) {
      return width / (2.0 * 0.6370702608);
    },
    // Canonical 35-point OpenVINO ADAS face model (Nose Tip Pt 5 is Origin (0,0,0))
    MODEL_POINTS_35: [
      [  -15.0,   -32.0,    35.0], // Pt 0
      [  -46.0,   -32.0,    43.0], // Pt 1
      [   15.0,   -32.0,    35.0], // Pt 2
      [   46.0,   -32.0,    43.0], // Pt 3
      [    0.0,   -22.0,    20.0], // Pt 4
      [    0.0,     0.0,     0.0], // Pt 5
      [  -16.0,     6.0,    20.0], // Pt 6
      [   16.0,     6.0,    20.0], // Pt 7
      [  -25.0,    32.0,    30.0], // Pt 8
      [   25.0,    32.0,    30.0], // Pt 9
      [    0.0,    26.0,    23.0], // Pt 10
      [    0.0,    40.0,    27.0], // Pt 11
      [  -12.0,   -48.0,    35.0], // Pt 12
      [  -32.0,   -52.0,    40.0], // Pt 13
      [  -50.0,   -48.0,    43.0], // Pt 14
      [   12.0,   -48.0,    35.0], // Pt 15
      [   32.0,   -52.0,    40.0], // Pt 16
      [   50.0,   -48.0,    43.0], // Pt 17
      [  -70.0,   -35.0,    70.0], // Pt 18
      [  -68.0,   -20.0,    65.0], // Pt 19
      [  -64.0,    -5.0,    59.0], // Pt 20
      [  -58.0,    12.0,    51.0], // Pt 21
      [  -50.0,    28.0,    43.0], // Pt 22
      [  -40.0,    44.0,    37.0], // Pt 23
      [  -28.0,    58.0,    33.0], // Pt 24
      [  -15.0,    68.0,    31.0], // Pt 25
      [    0.0,    70.0,    35.0], // Pt 26
      [   15.0,    68.0,    31.0], // Pt 27
      [   28.0,    58.0,    33.0], // Pt 28
      [   40.0,    44.0,    37.0], // Pt 29
      [   50.0,    28.0,    43.0], // Pt 30
      [   58.0,    12.0,    51.0], // Pt 31
      [   64.0,    -5.0,    59.0], // Pt 32
      [   68.0,   -20.0,    65.0], // Pt 33
      [   70.0,   -35.0,    70.0], // Pt 34
    ],
    // Canonical 5-point YuNet alignment model (Pt 0 is Nose Tip Origin (0,0,0))
    MODEL_POINTS_5: [
      [    0.0,     0.0,     0.0], // Pt 0
      [  -30.5,   -32.0,    39.0], // Pt 1
      [   30.5,   -32.0,    39.0], // Pt 2
      [  -25.0,    32.0,    30.0], // Pt 3
      [   25.0,    32.0,    30.0], // Pt 4
    ]
  };
});
