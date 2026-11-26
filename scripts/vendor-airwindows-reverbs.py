#!/usr/bin/env python3
"""Reproduce the SDK-free double-precision Airwindows cores from pinned sources."""
from pathlib import Path
import re
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
REVISION = "e68f75154ed0520fc5757fec9e7792dd1747e5ee"
DEST = ROOT / "NeuralAmpModeler/dsp/airwindows/reverbs"


def source(name, suffix):
    url = f"https://raw.githubusercontent.com/airwindows/airwindows/{REVISION}/plugins/LinuxVST/src/{name}/{name}{suffix}"
    return urlopen(url).read().decode("utf-8")


def generate(name):
    header, implementation, processor = (source(name, suffix) for suffix in (".h", ".cpp", "Proc.cpp"))
    constants = header[header.index("}; //") + len("}; //"):header.index("const int kNumPrograms")]
    constants = re.sub(r"^#define.*$", "", constants, flags=re.M)
    members = header[header.index("private:") + len("private:"):header.rindex("};")]
    members = re.sub(r"^.*(?:_programName|_canDo).*$", "", members, flags=re.M)
    members = re.sub(r"\bfloat ([A-F]);", r"double \1;", members)
    constructor = implementation[implementation.index(f"{name}::{name}("):implementation.index(f"{name}::~{name}")]
    reset = constructor[constructor.index("{") + 1:constructor.rindex("}")]
    reset = reset[:reset.index('_canDo.insert')]
    reset = re.sub(r"fpdL = 1\.0; while.*?;", "fpdL = 0x1745a23bu;", reset)
    reset = re.sub(r"fpdR = 1\.0; while.*?;", "fpdR = 0x62b7e149u;", reset)
    if name == "Galactic3":
        for size in (6480, 3660, 1720, 680, 9700, 6000, 2320, 940, 15220, 8460, 4540, 3200, 3111):
            reset = reset.replace(f"count < {size - 1};", f"count < {size};")
        reset = reset.replace("oldfpd = 429496.7295;", "oldfpd = 0.4294967295;", 1)
        reset = reset.replace("bez[bez_cycle] = 1.0;", "bez[bez_cycle] = 0.0;", 1)
    else:
        reset = re.sub(r"\+(\s*)2;", r"+\g<1>5;", reset)
    double = processor[processor.index(f"void {name}::processDoubleReplacing"):]
    prefix = double[double.index("double overallscale"):double.index("while (--sampleFrames")]
    coefficients = re.findall(r"\b(double|int|bool) (\w+)\s*=", prefix)
    prefix = re.sub(r"\b(?:double|int|bool) (\w+)\s*=", r"\1 =", prefix)
    body = double[double.index("{", double.index("while (--sampleFrames")) + 1:double.rindex("}")]
    body = body[:body.rindex("}")]
    body = body.replace("*in1", "left").replace("*in2", "right").replace("*out1", "left").replace("*out2", "right")
    body = re.sub(r"^\s*(?:in1|in2|out1|out2)\+\+;.*$", "", body, flags=re.M)
    if name == "Galactic3":
        # Scale predelay, drift, lowpass and fractional bez clock consistently across 44.1-192 kHz.
        prefix = prefix.replace(
            "\tregen = 0.0625+((1.0-A)*0.0625);\n"
            "\tattenuate = (1.0 - (regen / 0.125))*1.333;\n"
            "\tlowpass = pow(1.00001-(1.0-B),2.0)/sqrt(overallscale);\n"
            "\tdrift = pow(C,3)*0.001;\n"
            "\tderez = D/overallscale;\n"
            "\tif (derez < 0.0005) derez = 0.0005; if (derez > 1.0) derez = 1.0;\n"
            "\tderez = 1.0 / ((int)(1.0/derez));\n"
            "\t//this hard-locks derez to exact subdivisions of 1.0",
            "\tconst double rateScale = fmax(1.0, overallscale);\n"
            "\tregen = 0.0625+((1.0-A)*0.0625);\n"
            "\tattenuate = (1.0 - (regen / 0.125))*1.333;\n"
            "\tconst double baseLowpass = fmin(1.0, pow(1.00001-(1.0-B),2.0));\n"
            "\tlowpass = baseLowpass >= 1.0 ? 1.0 : 1.0 - pow(1.0 - baseLowpass, 1.0 / rateScale);\n"
            "\tdrift = pow(C,3)*0.001/rateScale;\n"
            "\tderez = 0.5*D/rateScale;\n"
            "\tif (derez < 0.00025) derez = 0.00025; if (derez > 0.5) derez = 0.5;",
            1,
        )
        prefix = prefix.replace("\tdelayM = 256;", "\tdelayM = (int)(256.0 * fmin(12.0, rateScale));", 1)
        body = body.replace(
            "\t\tdouble offsetML = (sin(vibM)+1.0)*127;\n"
            "\t\tdouble offsetMR = (sin(vibM+(3.141592653589793238/2.0))+1.0)*127;",
            "\t\tdouble offsetML = (sin(vibM)+1.0)*(delayM*0.49609375);\n"
            "\t\tdouble offsetMR = (sin(vibM+(3.141592653589793238/2.0))+1.0)*(delayM*0.49609375);",
            1,
        )
        body = body.replace(
            "\t\tbez[bez_cycle] += derez;\n"
            "\t\tbez[bez_SampL] += ((inputSampleL+bez[bez_InL]) * derez);\n"
            "\t\tbez[bez_SampR] += ((inputSampleR+bez[bez_InR]) * derez);\n"
            "\t\tbez[bez_InL] = inputSampleL; bez[bez_InR] = inputSampleR;\n"
            "\t\tif (bez[bez_cycle] > 1.0) { //hit the end point and we do a reverb sample\n"
            "\t\t\tbez[bez_cycle] = 0.0;",
            "\t\tbez[bez_cycle] += derez;\n"
            "\t\tconst double avgInL = inputSampleL + bez[bez_InL];\n"
            "\t\tconst double avgInR = inputSampleR + bez[bez_InR];\n"
            "\t\tbez[bez_InL] = inputSampleL; bez[bez_InR] = inputSampleR;\n"
            "\t\tif (bez[bez_cycle] >= 1.0) { //hit the end point and we do a reverb sample\n"
            "\t\t\tconst double over = bez[bez_cycle] - 1.0;\n"
            "\t\t\tconst double tailWeight = 2.0 * (derez - over);\n"
            "\t\t\tbez[bez_SampL] += avgInL * tailWeight;\n"
            "\t\t\tbez[bez_SampR] += avgInR * tailWeight;\n"
            "\t\t\tbez[bez_cycle] = over;",
            1,
        )
        body = body.replace(
            "\t\t\tbez[bez_SampL] = 0.0;\n\n"
            "\t\t\tbez[bez_CR] = bez[bez_BR];\n"
            "\t\t\tbez[bez_BR] = bez[bez_AR];\n"
            "\t\t\tbez[bez_AR] = inputSampleR;\n"
            "\t\t\tbez[bez_SampR] = 0.0;\n"
            "\t\t}",
            "\t\t\tbez[bez_SampL] = avgInL * (2.0 * over);\n\n"
            "\t\t\tbez[bez_CR] = bez[bez_BR];\n"
            "\t\t\tbez[bez_BR] = bez[bez_AR];\n"
            "\t\t\tbez[bez_AR] = inputSampleR;\n"
            "\t\t\tbez[bez_SampR] = avgInR * (2.0 * over);\n"
            "\t\t} else {\n"
            "\t\t\tbez[bez_SampL] += avgInL * (2.0 * derez);\n"
            "\t\t\tbez[bez_SampR] += avgInR * (2.0 * derez);\n"
            "\t\t}",
            1,
        )
    if name == "kGuitarHall2":
        # Upstream typo: bank 4 incremented c6ZBR a second time instead of c6ZAR,
        # leaving a6ZAR unincremented and skipping every other slot in a6ZBR.
        body = body.replace(
            "\t\t\tc6TR++; if (c6TR < 0 || c6TR > d6T) c6TR = 0;\n"
            "\t\t\tc6ZBR++; if (c6ZBR < 0 || c6ZBR > d6ZB) c6ZBR = 0;\n"
            "\t\t\tc6ZGR++; if (c6ZGR < 0 || c6ZGR > d6ZG) c6ZGR = 0;",
            "\t\t\tc6TR++; if (c6TR < 0 || c6TR > d6T) c6TR = 0;\n"
            "\t\t\tc6ZAR++; if (c6ZAR < 0 || c6ZAR > d6ZA) c6ZAR = 0;\n"
            "\t\t\tc6ZGR++; if (c6ZGR < 0 || c6ZGR > d6ZG) c6ZGR = 0;",
            1,
        )
    parameters = 5 if name == "kPlate140" else 6
    assignments = " ".join(f"{chr(65+i)} = controls[{i}];" for i in range(parameters))
    text = f'''#pragma once
// Copyright (c) Chris Johnson / Airwindows. MIT license in LICENSE.txt.
// Adapted from airwindows/airwindows @ {REVISION}.
// Generated by scripts/vendor-airwindows-reverbs.py: removes VST2 plumbing,
// seeds PRNGs deterministically, separates control coefficients from the sample
// clock, and uses doubles for controls. The double DSP path is otherwise retained.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <new>
#include <type_traits>
namespace airwindows_reverb::{name}
{{
{constants}
class Core
{{
public:
  Core() = default;
  void Reset(double rate)
  {{
    static_assert(std::is_trivially_destructible_v<Core>);
    ::new (this) Core{{}};
    mRate = std::isfinite(rate) ? std::clamp(rate, 1., 1'000'000.) : 48000.;
{reset}
  }}
  void Set(const std::array<double, {parameters}>& controls)
  {{
    {assignments}
{prefix}
  }}
  void Tick(double& left, double& right)
  {{
{body}
  }}
private:
  double getSampleRate() const {{ return mRate; }}
  double mRate = 48000.;
'''
    text += "\n".join(f"  {kind} {field}{{}};" for kind, field in coefficients)
    text += f"\n{members}\n}};\n}} // namespace airwindows_reverb::{name}\n"
    for macro, literal in [("M_PI_4", "0.78539816339744830962"), ("M_PI_2", "1.57079632679489661923"), ("M_PI", "3.14159265358979323846")]:
        text = text.replace(macro, literal)
    text = "\n".join(line.rstrip() for line in text.splitlines()) + "\n"
    (DEST / f"{name}.h").write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    DEST.mkdir(parents=True, exist_ok=True)
    for algorithm in ("kPlate140", "kGuitarHall2", "Galactic3"):
        generate(algorithm)
    license_url = f"https://raw.githubusercontent.com/airwindows/airwindows/{REVISION}/LICENSE"
    (DEST / "LICENSE.txt").write_bytes(urlopen(license_url).read())
