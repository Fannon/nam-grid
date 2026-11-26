#!/usr/bin/env python3
"""SDK-free outboard cores. Explicit deviations below; pinned like the reverbs.

Use --source-dir for an offline directory containing Plugin.h/.cpp/Proc.cpp.
"""
import argparse
from pathlib import Path
import re
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
REVISION = "e68f75154ed0520fc5757fec9e7792dd1747e5ee"
PLUGINS = ("CStrip2", "ButterComp2", "BussColors4", "DigitalBlack", "CStripGate")
DEST = ROOT / "NeuralAmpModeler/dsp/airwindows/outboard"


def _strip_right_channel(body):
    """Strip right-channel-only statements for TickMono generation."""
    right_tokens = (
        "right", "inputSampleR", "drySampleR", "highSampleR", "midSampleR", "bassSampleR",
        "gaterollerR", "ZeroCrossR", "WasNegativeR", "holdR", "fpdR", "dR[", "bR[",
        "controlR", "slowdynR", "dynamicconvR", "applyconvR", "totalmultiplierR",
        "inputposR", "outputposR", "targetposR", "invposR", "calcposR",
        "inputnegR", "outputnegR", "targetnegR", "invnegR", "calcnegR",
        "controlAposR", "controlBposR", "controlAnegR", "controlBnegR", "lastOutputR",
        "avgRA", "nvgRA", "lastSampleR", "last2SampleR", "tripletFactorR",
        "tripletRA", "tripletRB", "tripletRC", "monitor1 =",
        "iirHighpassAR", "iirHighpassBR", "iirHighpassCR", "iirHighpassDR", "iirHighpassER", "iirHighpassFR",
        "iirLowpassAR", "iirLowpassBR", "iirLowpassCR", "iirLowpassDR", "iirLowpassER", "iirLowpassFR",
        "iirHighSampleR", "iirLowSampleR", "phatSampleR", "distSampleR", "rawSampleR",
    )
    lines = []
    skip_brace_depth = 0
    for line in body.splitlines():
        if skip_brace_depth > 0:
            skip_brace_depth += line.count("{") - line.count("}")
            continue
        if any(tok in line for tok in right_tokens):
            if "{" in line and line.count("{") > line.count("}"):
                skip_brace_depth = line.count("{") - line.count("}")
            continue
        lines.append(line)
    text = "\n".join(lines)
    text = re.sub(r"monitor0 = monitor1 = 1\.;", "monitor0 = 1.;", text)
    return text


def generate(name, source):
    if name == "CStripGate":
        generate_gate(source)
        return
    header, implementation, processor = (source(name, s) for s in (".h", ".cpp", "Proc.cpp"))
    parameters = int(re.search(r"kNumParameters\s*=\s*(\d+)", header)[1])
    members = header.split("private:")[1].rsplit("};", 1)[0]
    members = re.sub(r"^.*(?:_programName|_canDo).*$", "", members, flags=re.M)
    members = re.sub(r"\bfloat ([A-J]);", r"double \1;", members)
    ctor = implementation.split(f"{name}::{name}(")[1].split(f"{name}::~")[0]
    reset = ctor[ctor.index("{") + 1:ctor.index("_canDo.insert")]
    reset = re.sub(r"fpdL = 1\.0; while.*?;", "fpdL = 0x1745a23bu;", reset)
    reset = re.sub(r"fpdR = 1\.0; while.*?;", "fpdR = 0x62b7e149u;", reset)
    double = processor.split(f"void {name}::processDoubleReplacing")[1]
    prefix = double[double.index("double overallscale"):double.index("while (--sampleFrames")]
    # Variables declared outside the original loop become core members. Loop
    # temporaries and coefficients keep their original evaluation order.
    fields = re.findall(r"\b(?:const )?(double|int|bool) (\w+)\s*(?:=|;)", prefix)
    prefix = re.sub(r"\b(?:const )?(?:double|int|bool) (\w+)(?=\s*[=;])", r"\1", prefix)
    body = double[double.index("{", double.index("while (--sampleFrames")) + 1:double.rindex("}")]
    body = body[:body.rindex("}")]
    body = re.sub(r"^\s*\*?(?:in1|in2|out1|out2)\+\+;.*$", "", body, flags=re.M)
    for old, new in (("*in1", "left"), ("*in2", "right"), ("*out1", "left"), ("*out2", "right")):
        body = body.replace(old, new)
    public = ""
    extra = ""
    includes = ""
    helpers = ""
    mono_body = ""
    if name == "DigitalBlack":
        # Counters measure reference samples, not host samples. Scaling only
        # release leaves the zero-crossing hold twice as long at twice the rate.
        members = re.sub(r"int (ZeroCross[LR]);", r"double \1;", members)
        body = body.replace("ZeroCrossL += 1;", "ZeroCrossL += 44100.0 / mRate;")
        body = body.replace("ZeroCrossR += 1;", "ZeroCrossR += 44100.0 / mRate;")
        for ch in "LR":
            old_gate = (
                f"\t\tdouble gate = 1.0;\n" if ch == "L" else "\t\tgate = 1.0;\n"
            ) + (
                f"\t\tif (gateroller{ch} < 1.0) gate = gateroller{ch};\n\n"
                f"\t\t{'double ' if ch == 'L' else ''}bridgerectifier = 1-cos(fabs(inputSample{ch}));\n\n"
                f"\t\tif (inputSample{ch} > 0) inputSample{ch} = (inputSample{ch}*gate)+(bridgerectifier*(1-gate));\n"
                f"\t\telse inputSample{ch} = (inputSample{ch}*gate)-(bridgerectifier*(1-gate));\n\n"
                f"\t\tif ((gate == 0.0) && (fabs(inputSample{ch}*3) < threshold)) inputSample{ch} = 0.0;"
            )
            new_gate = (
                f"\t\t{'double ' if ch == 'L' else ''}gate = 1.0;\n"
                f"\t\tif (gateroller{ch} < 1.0)\n"
                f"\t\t{{\n"
                f"\t\t\tgate = gateroller{ch};\n"
                f"\t\t\t{'double ' if ch == 'L' else ''}bridgerectifier = 1-cos(fabs(inputSample{ch}));\n"
                f"\t\t\tif (inputSample{ch} > 0) inputSample{ch} = (inputSample{ch}*gate)+(bridgerectifier*(1-gate));\n"
                f"\t\t\telse inputSample{ch} = (inputSample{ch}*gate)-(bridgerectifier*(1-gate));\n"
                f"\t\t\tif ((gate == 0.0) && (fabs(inputSample{ch}*3) < threshold)) inputSample{ch} = 0.0;\n"
                f"\t\t}}"
            )
            body = body.replace(old_gate, new_gate)
        public = "double Opening(int channel) const { return fmin(1., channel == 0 ? gaterollerL : gaterollerR); }"
        mono_raw = body
        mono_raw = re.sub(r"\t\tif \(inputSampleR > 0\).*?else \{\n.*?\n\t\t\}\n", "", mono_raw, flags=re.S)
        mono_raw = re.sub(r"\t\tif \(fabs\(inputSampleR\) > threshold\).*?else \{\n\t\t\tgaterollerR -= release;\n\t\t\}\n", "", mono_raw, flags=re.S)
        mono_body = _strip_right_channel(mono_raw)
    if name == "BussColors4":
        reset = reset.replace(
            "for (int count = 0; count < 174; count++) {bL[count] = 0; bR[count] = 0;}",
            "for (int count = 0; count < 256; count++) {bL[count] = 0; bR[count] = 0;}\n\tbIndex = 0;",
        )
        members = members.replace("double bL[175];", "double bL[256];").replace(
            "double bR[175];", "double bR[256];\n\tint bIndex = 0;"
        )
        body = re.sub(
            r"\t\tfor \(int count = maxConvolutionBufferSize; count > 0; --count\) \{bL\[count\] = bL\[count-1\];\}.*?\n\t\tbL\[0\] = inputSampleL;\n\n\t\tfor \(int count = maxConvolutionBufferSize; count > 0; --count\) \{bR\[count\] = bR\[count-1\];\}.*?\n\t\tbR\[0\] = inputSampleR;",
            "\t\tbIndex = (bIndex - 1) & 255;\n\t\tbL[bIndex] = inputSampleL;\n\t\tbR[bIndex] = inputSampleR;",
            body,
            flags=re.S,
        )
        body = re.sub(r"\bb([LR])\[c\[(\d+)\]\]", r"b\1[(bIndex + c[\2]) & 255]", body)
        mono_raw = body
        mono_raw = re.sub(r"\t\tbridgerectifier = fabs\(inputSampleR\);\n\t\tslowdynR.*?//end pre saturation stage R\n", "", mono_raw, flags=re.S)
        mono_raw = re.sub(r"\t\tdR\[gcount\+44\].*?//now we have a 'sag' style average to apply to the conv, R\n", "", mono_raw, flags=re.S)
        mono_raw = re.sub(r"\t\tbridgerectifier = fabs\(inputSampleR\);\n\t\tbridgerectifier = 1\.0-cos\(bridgerectifier\);\n\t\tif \(inputSampleR > 0\) inputSampleR -= bridgerectifier;\n\t\telse inputSampleR \+= bridgerectifier;\n", "", mono_raw)
        mono_body = _strip_right_channel(mono_raw)
    if name == "ButterComp2":
        # Replace function-static, signed-overflow residue noise with the core's
        # deterministic local PRNG. No shared mutable state or undefined overflow.
        start = body.index("static int noisesourceL")
        end = body.index("double drySampleL", start)
        body = body[:start] + """if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
        """ + body[end:]
        for ch in "LR":
            body = body.replace(f"inputSample{ch} *= totalmultiplier{ch};",
                                f"monitor{'0' if ch == 'L' else '1'} = totalmultiplier{ch};\n\t\tinputSample{ch} *= totalmultiplier{ch};")
            for sign in ("pos", "neg"):
                target = f"target{sign}{ch}"
                inv = f"inv{sign}{ch}"
                calc = f"calc{sign}{ch}"
                body = body.replace(
                    f"double {calc} = pow((1.0/{target}),2);",
                    f"if (!({target} >= 1.e-12)) {target} = 1.e-12;\n\t\tconst double {inv} = 1.0 / {target};\n\t\tdouble {calc} = {inv} * {inv};",
                )
        mono_raw = re.sub(r"\t\tif \(inputSampleR > 0\).*?//this causes each of the four to update only when active and in the correct 'flip'\n", "", body, flags=re.S)
        mono_body = _strip_right_channel(mono_raw)
    if name == "CStrip2":
        # Capacitor pole compensation preserves open/off exactly. Percentages
        # remain native controls, NOT a claim of a calibrated Hz cutoff.
        prefix += """
    lowpassAmount = 1.0 - pow(1.0 - lowpassAmount, 44100.0 / mRate);
    highpassAmount = 1.0 - pow(1.0 - highpassAmount, 44100.0 / mRate);
"""
        # Smooth native Mid, Capacitor High Cut and output/Spiral drive over
        # the wrapper's 32-sample coefficient quantum, preserving settled math.
        includes = '#include "CoefficientRamp.h"'
        prefix += "    mCoefficientRamp.Target({densityB, lowpassAmount, outputgain});\n    ApplySmoothedCoefficients();\n"
        helpers = """  // Interpolate the quantities that step during Mid, High Cut and Saturate
  // automation. Recompute dependent absolute values/branches after interpolation
  // so crossing zero does not itself introduce a discontinuity.
  void ApplySmoothedCoefficients()
  {
    const auto& values = mCoefficientRamp.Values();
    densityB = values[0]; lowpassAmount = values[1]; outputgain = values[2];
    outB = fabs(densityB);
    density = outputgain - 1.0;
    if (density < 0.0) density = 0.0;
    phattity = density - 1.0;
    if (density > 1.0) density = 1.0;
    if (phattity < 0.0) phattity = 0.0;
    engageEQ = densityA != 0.0 || densityB != 0.0 || densityC != 0.0;
    engageCapacitor = lowpassAmount < 1.0 || highpassAmount > 0.0;
  }
  CoefficientRamp<3> mCoefficientRamp;
"""
        body = "if (mCoefficientRamp.Tick()) ApplySmoothedCoefficients();\nmonitor0 = monitor1 = 1.;\n" + body
        for ch, idx in (("L", 0), ("R", 1)):
            body = body.replace(f"inputSample{ch} *= totalmultiplier;",
                                f"monitor{idx} = totalmultiplier;\n\t\t\tinputSample{ch} *= totalmultiplier;")
            for sign in ("pos", "neg"):
                target = f"target{sign}{ch}"
                inv = f"inv{sign}{ch}"
                calc = f"calc{sign}"
                body = body.replace(
                    f"{calc} = pow((1.0/{target}),2);",
                    f"if (!({target} >= 1.e-12)) {target} = 1.e-12;\n\t\t\tconst double {inv} = 1.0 / {target};\n\t\t\t{calc} = {inv} * {inv};",
                )
        for band, out_var, dens_var in (("high", "outA", "densityA"), ("mid", "outB", "densityB"), ("bass", "outC", "densityC")):
            old_band = (
                f"\t\t\t{band}SampleL *= ({dens_var}+1.0);\n"
                f"\t\t\tbridgerectifier = fabs({band}SampleL)*1.57079633;\n"
                f"\t\t\tif (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;\n"
                f"\t\t\t//max value for sine function\n"
                f"\t\t\tif ({dens_var} > 0) bridgerectifier = sin(bridgerectifier);\n"
                f"\t\t\telse bridgerectifier = 1-cos(bridgerectifier);\n"
                f"\t\t\t//produce either boosted or starved version\n"
                f"\t\t\tif ({band}SampleL > 0) {band}SampleL = ({band}SampleL*(1-{out_var}))+(bridgerectifier*{out_var});\n"
                f"\t\t\telse {band}SampleL = ({band}SampleL*(1-{out_var}))-(bridgerectifier*{out_var});\n"
                f"\t\t\t//blend according to {dens_var} control\n\n"
                f"\t\t\t{band}SampleR *= ({dens_var}+1.0);\n"
                f"\t\t\tbridgerectifier = fabs({band}SampleR)*1.57079633;\n"
                f"\t\t\tif (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;\n"
                f"\t\t\t//max value for sine function\n"
                f"\t\t\tif ({dens_var} > 0) bridgerectifier = sin(bridgerectifier);\n"
                f"\t\t\telse bridgerectifier = 1-cos(bridgerectifier);\n"
                f"\t\t\t//produce either boosted or starved version\n"
                f"\t\t\tif ({band}SampleR > 0) {band}SampleR = ({band}SampleR*(1-{out_var}))+(bridgerectifier*{out_var});\n"
                f"\t\t\telse {band}SampleR = ({band}SampleR*(1-{out_var}))-(bridgerectifier*{out_var});\n"
                f"\t\t\t//blend according to {dens_var} control"
            )
            indented = "\n".join("\t" + l if l else "" for l in old_band.splitlines())
            body = body.replace(old_band, f"\t\t\tif ({out_var} != 0.0) {{\n{indented}\n\t\t\t}}")
        old_sat = """\t\t\tdouble phatSampleL = sin(inputSampleL * 1.57079633);
\t\t\tdouble phatSampleR = sin(inputSampleR * 1.57079633);
\t\t\tinputSampleL *= 1.2533141373155;
\t\t\tinputSampleR *= 1.2533141373155;
\t\t\t//clip to 1.2533141373155 to reach maximum output, or 1.57079633 for pure sine 'phat' version
\t\t\tdouble distSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((fabs(inputSampleL) == 0.0) ?1.0:fabs(inputSampleL));
\t\t\tdouble distSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((fabs(inputSampleR) == 0.0) ?1.0:fabs(inputSampleR));
\t\t\tinputSampleL = distSampleL; //purest form is full Spiral
\t\t\tinputSampleR = distSampleR; //purest form is full Spiral
\t\t\tif (density < 1.0) {
\t\t\t\tinputSampleL = (rawSampleL*(1.0-density))+(distSampleL*density);
\t\t\t\tinputSampleR = (rawSampleR*(1.0-density))+(distSampleR*density);
\t\t\t} //fade Spiral aspect"""
        new_sat = """\t\t\tdouble phatSampleL = 0.0, phatSampleR = 0.0;
\t\t\tif (phattity > 0.0) {
\t\t\t\tphatSampleL = sin(inputSampleL * 1.57079633);
\t\t\t\tphatSampleR = sin(inputSampleR * 1.57079633);
\t\t\t}
\t\t\tif (density > 0.0) {
\t\t\t\tinputSampleL *= 1.2533141373155;
\t\t\t\tinputSampleR *= 1.2533141373155;
\t\t\t\t//clip to 1.2533141373155 to reach maximum output, or 1.57079633 for pure sine 'phat' version
\t\t\t\tdouble distSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((fabs(inputSampleL) == 0.0) ?1.0:fabs(inputSampleL));
\t\t\t\tdouble distSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((fabs(inputSampleR) == 0.0) ?1.0:fabs(inputSampleR));
\t\t\t\tinputSampleL = distSampleL; //purest form is full Spiral
\t\t\t\tinputSampleR = distSampleR; //purest form is full Spiral
\t\t\t\tif (density < 1.0) {
\t\t\t\t\tinputSampleL = (rawSampleL*(1.0-density))+(distSampleL*density);
\t\t\t\t\tinputSampleR = (rawSampleR*(1.0-density))+(distSampleR*density);
\t\t\t\t} //fade Spiral aspect
\t\t\t} else {
\t\t\t\tinputSampleL = rawSampleL;
\t\t\t\tinputSampleR = rawSampleR;
\t\t\t}"""
        body = body.replace(old_sat, new_sat)
        mono_raw = re.sub(r"\t\t\t//begin R\n.*?//end R\n", "", body, flags=re.S)
        mono_raw = re.sub(r"\t\t\t\tif \(true == flip\)\n\t\t\t\t\{[^}]*controlA(?:pos|neg)R[^}]*\}\n\t\t\t\telse\n\t\t\t\t\{[^}]*controlB(?:pos|neg)R[^}]*\}\n", "", mono_raw, flags=re.S)
        mono_body = _strip_right_channel(mono_raw)
    if name in ("ButterComp2", "CStrip2"):
        public = "double Gain(int channel) const { return channel == 0 ? monitor0 : monitor1; }"
        extra = "double monitor0 = 1., monitor1 = 1.;"
    assignments = " ".join(f"{chr(65+i)} = controls[{i}];" for i in range(parameters))
    mono_body = _strip_right_channel(re.sub(r"\t\t\tif \(gateR == 0\.0\).*?else\n\t\t\t\{gateR = 1\.0;\}", "", body, flags=re.S))
    text = f'''#pragma once
// Copyright (c) Chris Johnson / Airwindows. MIT license in LICENSE.txt.
// Source: airwindows/airwindows @ {REVISION}.
// Generated by scripts/vendor-airwindows-outboard.py. See its explicit
// Capacitor rate compensation, coefficient interpolation, local noise and gain-monitor adaptations.
#include <array>
#include <cmath>
#include <cstdint>
{includes}
namespace airwindows_outboard::{name}
{{
class Core
{{
public:
  void Reset(double rate)
  {{
    *this = Core{{}};
    mRate = rate;
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
  void TickMono(double& left)
  {{
{mono_body}
  }}
  {public}
private:
{helpers}  double getSampleRate() const {{ return mRate; }}
  double mRate = 48000.;
  {extra}
'''
    text += "\n".join(f"  {kind} {field}{{}};" for kind, field in fields)
    text += f"\n{members}\n}};\n}} // namespace airwindows_outboard::{name}\n"
    text = text.replace("M_PI_2", "1.57079632679489661923")
    text = "\n".join(line.rstrip() for line in text.splitlines()) + "\n"
    (DEST / f"{name}.h").write_text(text, encoding="utf-8", newline="\n")


def generate_gate(source):
    """Extract only CStrip's gate; no EQ, compressor, lowpass or TimeLag."""
    processor = source("CStrip", "Proc.cpp").split("void CStrip::processDoubleReplacing")[1]
    body = processor.split("//begin Gate")[-1].split("//end Gate, begin antialiasing")[0]
    body = body.replace("ZeroCrossL += 1;", "ZeroCrossL += step;")
    body = body.replace("ZeroCrossR += 1;", "ZeroCrossR += step;")
    # Preserve the original integer truncations at its 44.1 kHz reference.
    body = body.replace("= absmax * 0.3;", "= floor(absmax * 0.3);")
    body = body.replace("= absmax;", "= floor(absmax);")
    mono_body = _strip_right_channel(re.sub(r"\t\t\tif \(gateR == 0\.0\).*?else\n\t\t\t\{gateR = 1\.0;\}", "", body, flags=re.S))
    text = f'''#pragma once
// Copyright (c) Chris Johnson / Airwindows. MIT license in LICENSE.txt.
// CStrip gate only @ {REVISION}.
// Generated by scripts/vendor-airwindows-outboard.py. Release and counters
// use reference-sample units for rate compensation; 44.1 kHz is unchanged.
#include <cmath>
#include <cstdint>
namespace airwindows_outboard::CStripGate
{{
class Core
{{
public:
  void Reset(double rate) {{ *this = Core{{}}; step = 44100.0 / rate; }}
  void Set(double threshold) {{ onthreshold = threshold; offthreshold = threshold * 1.1; release = 0.028331119964586 * step; }}
  void Tick(double& inputSampleL, double& inputSampleR)
  {{
    // The positive denormal guard also sets zero-crossing state during silence;
    // removing it changes which half-cycle first opens the native gate.
    if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
    if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
{body}
    fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
    fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
  }}
  void TickMono(double& inputSampleL)
  {{
    if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
{mono_body}
    fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
  }}
  double Opening(int channel) const {{ return channel == 0 ? gateL : gateR; }}
private:
  double onthreshold = 0.001778279410038923, offthreshold = onthreshold * 1.1;
  double step = 1., release = 0.028331119964586, absmax = 220.9, bridgerectifier = 0.;
  bool engageGate = true, WasNegativeL = false, WasNegativeR = false;
  uint32_t fpdL = 0x1745a23bu, fpdR = 0x62b7e149u;
  double ZeroCrossL = 0., ZeroCrossR = 0., gaterollerL = 0., gaterollerR = 0., gateL = 0., gateR = 0.;
}};
}} // namespace airwindows_outboard::CStripGate
'''
    text = "\n".join(line.rstrip() for line in text.splitlines()) + "\n"
    (DEST / "CStripGate.h").write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugins", nargs="+", choices=PLUGINS, default=PLUGINS)
    parser.add_argument("--source-dir", type=Path)
    args = parser.parse_args()
    def source(name, suffix):
        if args.source_dir:
            return (args.source_dir / (name + suffix)).read_text(encoding="utf-8")
        return urlopen(f"https://raw.githubusercontent.com/airwindows/airwindows/{REVISION}/plugins/LinuxVST/src/{name}/{name}{suffix}").read().decode("utf-8")
    DEST.mkdir(parents=True, exist_ok=True)
    for plugin in args.plugins:
        generate(plugin, source)
    # Same author/license as the already-vendored pinned reverb cores.
    (DEST / "LICENSE.txt").write_bytes((DEST.parent / "reverbs/LICENSE.txt").read_bytes())
