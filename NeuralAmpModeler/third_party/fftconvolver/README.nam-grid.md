# FFTConvolver subset

Source: https://github.com/HiFi-LoFi/FFTConvolver
Revision: f2cdeb04c42141d2caec19ca4f137398b2a76b85
License: MIT, retained in COPYING.txt and individual source headers.

Only the uniform convolver, bundled AudioFFT and utilities are used. The two-stage
engine is not included. SSE accumulation is enabled on x64; other architectures
retain the portable implementation. The bundled Ooura FFT backend is used.

Local modification in FFTConvolver.cpp: the initializer removes only exact-zero
trailing coefficients, preserving quiet nonzero tails. This is necessary to keep
the existing cabinet response; see [adaptive IR design](../../../docs/adaptive-ir.md).
Trailing whitespace is normalized to the repository's formatting rules.
