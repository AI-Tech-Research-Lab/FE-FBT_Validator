# NEXT: CKKS level calculator

## Idea

Add a calculator to the page that, given the Fourier degree d and the CKKS configuration, tells the user **when and where FE-FBT can be used** compared with normal bootstrapping. It returns three rows:

| Operation | Minimum effective input levels | Last callable level, scale degree 1 / 2 | Effective levels in output |
|---|---:|---:|---:|
| FE-FBT, degree d | … | … | … |
| Normal BS, 1 iteration | … | … | … |
| Normal BS, 2 iterations | … | … | … |

- The first column counts what happens **before** the modulus raise (for FE-FBT, the SlotsToCoeffs runs before ModRaise). The third column counts what happens **after**.
- Last level = D − minimum effective. With scale degree 2 (not yet rescaled) one more level is needed.
- If the user also enters the current ciphertext's level and scale degree, each row says whether it can be called right away (yes/no).
- If the FE-FBT output is ≤ 0 levels, the row shows "not feasible with this chain" plus the minimum D needed.

Reference example (from the original note, Kumo-S profile, D = 27, degree 27): FE-FBT 6 / 21–20 / 7; BS 1 iteration 1 / 26–25 / 8; BS 2 iterations 1 / 26–25 / 7. Going from degree 27 to 512 only changes the FE-FBT output (7 → 2). (The Kumo-S parameters are unknown; it is not needed.)

## Decisions already made

1. **Source of truth:** OpenFHE **v1.6.1** (contains `GetFEFBTDepth`, `EvalFEFuncBootstrap`). Clone and build it locally in `openfhe/` (in `.gitignore`):
   ```
   git clone --depth 1 --branch v1.6.1 https://github.com/openfheorg/openfhe-development.git openfhe
   cd openfhe && mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_UNITTESTS=OFF -DBUILD_BENCHMARKS=OFF -DCMAKE_INSTALL_PREFIX=$PWD/../install
   make -j && make install
   ```
2. **Inputs** (chosen by us, as sliders/selectors):
   - chain depth D (slider, e.g. 8…60);
   - number of slots, log₂ (slider 3…16), which caps the budgets at logSlots;
   - scaling technique: FLEXIBLEAUTO (default), FLEXIBLEAUTOEXT, FIXEDAUTO, FIXEDMANUAL (no composite scaling);
   - secret key distribution: SPARSE_ENCAPSULATED (default), SPARSE_TERNARY, UNIFORM_TERNARY;
   - first modulus q₀: "≤ 60 bits" / "> 60 bits", which only matters for SPARSE_ENCAPSULATED (> 60 → Hamming weight 64 → K = 28 tables);
   - budget {CtS, StC} for FE-FBT and budget {CtS, StC} for normal BS (sliders 1…5, capped at logSlots);
   - Fourier degree: **linked to the page's degree slider**;
   - optional: current ciphertext level + scale degree 1/2.
3. **Degree:** the current slider (2⁴…2¹⁵) is too large for real HE: lower it. Proposal: a list of values that includes the Paterson–Stockmeyer "sweet spots" (largest degree at equal depth) and the powers of two:
   `5, 8, 13, 16, 27, 32, 59, 64, 119, 128, 247, 256, 495, 512, 1007, 1024`, default 119. The worker already works with any d (it does not require powers of two).
4. **Layout:** the inputs go in a collapsible group (`<details>`) in the sidebar, right **below the FE-FBT controls** (after "Hermite order"). The results table goes in the main area, below the statistics, and appears when the group is open.
5. **Edge cases:** confirmed "not feasible" plus the minimum D.

## Formulas found in OpenFHE v1.6.1 (`src/pke/lib/scheme/ckksrns/ckksrns-fhe.cpp`)

- `GetDepthByDegree(d)` (ckksrns-utils.cpp): 0→0, 1→1, 2→2, 3–4→3, 5→4, 6–13→5, 14–27→6, 28–59→7, 60–119→8, 120–247→9, 248–495→10, 496–1007→11, 1008–2031→12, 2032–4031→13, …
- `GetMultiplicativeDepthByCoeffVector(v, normalized) = GetDepthByDegree(v.size()−1) − normalized`.
- **GetFEFBTDepth** = CtS + StC + depth(exp table) + R + (GetDepthByDegree(d) − 1)
  - UNIFORM_TERNARY: exp table of degree 27 (depth 6), R = 9 → 15
  - SPARSE_TERNARY, or SPARSE_ENCAPSULATED with q₀ > 60: degree 48 (depth 7), R = 3 → 10
  - SPARSE_ENCAPSULATED with q₀ ≤ 60 (Hamming weight 32): degree 23 (depth 6), R = 4 → 10
- **GetBootstrapDepth** = CtS + StC + modDepth; sparse (ST and SE) = 7 + 3 = 10, uniform = 8 + 6 = 14. It does not take q₀ (for SE with Hamming weight 32 the table has degree 36, which still has depth 7, so no difference).
- Budgets are capped to [1, logSlots] in the setup.
- FE-FBT requires **q₀ = Δ + 1 bits** (`FirstModSize = ScalingModSize + 1`); otherwise it throws.
- FE-FBT input check: `level + (noiseScaleDeg−1) ≤ L0 − (StC + 2) − [FLEXIBLEAUTOEXT]`, with L0 = number of towers = D + 1 (+1 with FLEXIBLEAUTOEXT).
- The pre-ModRaise phase of FE-FBT is StC + correction (AdjustCiphertext), and **the output is D − (GetFEFBTDepth − StC)**, as the original note said.

## Measured empirically so far (`tools/levelprobe`)

Program `tools/levelprobe/levelprobe.cpp`: for each input level and scale degree, it runs the real operation, checks the decryption and counts the effective levels in input and output (multiplications by 1.0 that still decrypt correctly). `grid.py` runs it over a grid of configurations. Build: `cmake -S tools/levelprobe -B tools/levelprobe/build && cmake --build tools/levelprobe/build`.

| Config | Result (last s1/s2, min eff. in, eff. out) | Formula |
|---|---|---|
| FE-FBT FLEXIBLEAUTO SE, D=24, b={2,2}, d=27, N=2¹², 2¹¹ slots | 21/20, 3, 7 (GetFEFBTDepth 19) | out = 24 − (19 − 2) ✓, min = StC + 1 |
| FE-FBT FLEXIBLEAUTO SE and ST, D=30, b={3,2}, d=27 | 27/26, 3, 12 (GetFEFBTDepth 20) | 30 − 18 ✓ |
| FE-FBT FLEXIBLEAUTO UNIFORM, D=30, b={3,2}, d=27 | 27/26, 3, 7 (GetFEFBTDepth 25) | 30 − 23 ✓ |
| FE-FBT FLEXIBLEAUTOEXT SE, D=30, b={3,2}, d=27 | 27/26, **4**, 12 | min eff. StC + 2 (extra tower), same last level |
| FE-FBT FLEXIBLEAUTO SE, D=30, b={1,1}, d=27, N=2¹⁰, 2⁵ slots | 28/–, 2, 14 (GetFEFBTDepth 17) | 30 − 16 ✓ |
| BS 1 iteration, FLEXIBLEAUTO SE q₀=51, D=24, b={2,2} | 23/22, 1, 10 (GetBootstrapDepth 14) | out = D − GetBootstrapDepth |
| BS 1 iteration, FIXEDMANUAL SE q₀=60, D=30, b={1,1}, 2⁵ slots | 29/28, 1, 18 (GetBootstrapDepth 12) | 30 − 12 ✓ |

## Still to verify (one process at a time!)

- FE-FBT with FIXEDAUTO and FIXEDMANUAL (min in, out).
- Degrees < 5 (d = 1…4): the "correction for degrees below 5" (PS is not used there; check whether the −1 of `normalized` still holds).
- Degrees 5, 6, 13, 14, 59, 119, 247 to check the transitions of the table.
- SE with q₀ = 61 (Hamming weight 64) for FE-FBT (Δ = 60) and for BS.
- BS 1 and 2 iterations for all scaling techniques / distributions; how much the 2nd iteration costs (the original note: −1 in output).
- Sparse vs full slots (does it change anything for the levels?).
- Also note how to define "effective levels" for FLEXIBLEAUTOEXT (extra tower: effective ≠ D − level).

⚠️ **Memory:** do not run probes in parallel. Use small N (2¹⁰) and few slots (2⁵): a run then takes ~100 MB and < 1 s. Budget {1,1} with many slots (dense linear transform) takes GB: avoid it. The `grid.py` in the repo still uses N = 2¹², 2¹¹ slots and 5 jobs: **change it** (jobs = 1, logN 10, logSlots 5/9) before running it.

## Then

1. Port the formulas to JS in `index.html` (pure function `levelRows(params, d)`), check them against the levelprobe output.
2. UI: the collapsible group + the table, the new degree list, README update (also fix "Open `main.html`" → `index.html`).
