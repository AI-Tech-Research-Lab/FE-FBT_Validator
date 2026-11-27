# FE-FBT Validator

An interactive, single-page tool to explore the Fourier-extension approximation used by FE-FBT: how a degree-*d* Fourier series, built on a period-2 Hermite extension of *f*, approximates *f* on a domain [L, U], and what that costs in terms of coefficient size and estimated CKKS noise.

## A message from humans, to humans
We did this tool to help us understand the capabilities of Fourier Extension Functional Bootstrapping, a recent technique that has been added to HE libraries such as OpenFHE and FIDESlib. This is meant to be exploratory and is vibe-coded, so take this with a grain of salt. 

Let us know if you find any problem or have any suggestion. 

## Running it

Open `index.html` in a modern browser. There is no build step and no server.

The page loads [decimal.js](https://github.com/MikeMcl/decimal.js) and [Plotly](https://plotly.com/javascript/) from cdnjs, and its fonts from Google Fonts, so it needs an internet connection the first time.

## Controls

| Control | Meaning |
| --- | --- |
| **Function** | A preset (GELU, sin, cos, exp, 1/√x, 1/x, sigmoid, tanh, ReLU) or any expression in `x`; see [Custom functions](#custom-functions). |
| **Domain** | *Symmetric* [−2ᵏ, 2ᵏ] or *Positive* [2⁻ᵏ, 2ᵏ], with k = 0…15. |
| **Series degree** | d ∈ {5, 8, 13, 16, 27, 32, 59, 64, 119, 128, 247, 256, 495, 512, 1007, 1024}, default 119: the Paterson–Stockmeyer "sweet spots" (the largest degree at a given depth) and the powers of two. |
| **Hermite order** | The extension order ke, chosen automatically or set by hand (1…40). |
| **x axis** | Linear, or logarithmic (positive domains only). |
| **Show the extension** | Also plots the extension p(s) and the series on [U, U + W]. |
| **Error panel** | Absolute error, relative error, or bits of precision. |
| **CKKS levels in OpenFHE** | A collapsible group with the [level calculator](#ckks-level-calculator). |

The statistics below the plot report the chosen ke, the max, mean and min error on [L, U], the bits of precision −log₂(err max / max|f|), the coefficient norm l1 = 2·Σ|cⱼ|, the estimated CKKS noise 8.5·10⁻¹⁰·l1, and the expected total bits −log₂((err max + noise) / max|f|). When ke is automatic, the "ke search" table lists every value that was tried.

## Method

With W = U − L, the function is rewritten on a period-2 variable m:

```
g(m) = f(L + W(m + ½)) ≈ 2 Re Σ_{j=0..d} c_j e^{iπjm}
```

On m ∈ [−½, ½) g is f on [L, U]. On m ∈ [½, 3/2) g is the Hermite extension

```
p(s) = (1 − s)^ke · T_A(s) + s^ke · T_B(s − 1),   s ∈ [0, 1],
```

which leaves f at U and returns to f at L, matching the derivatives of f up to order ke − 1 at both ends. The periodic function is therefore C^(ke−1), which is what makes its Fourier coefficients decay.

1. **Taylor data.** The scaled Taylor coefficients aᵢ = f⁽ⁱ⁾(U)·Wⁱ/i! and bᵢ = f⁽ⁱ⁾(L)·Wⁱ/i! are computed in 110-digit decimal arithmetic. T_A and T_B follow from them through binomial sums and are then rounded to double.
2. **Coefficients.** g is sampled on M points of one period (M ≥ 32d, at least 2¹⁶, at most 2²¹). An FFT gives c₀…c_d.
3. **Evaluation.** The series is evaluated with an inverse FFT on P points (P ≥ 4(d + 1), at least 2¹⁵) and compared with f in double precision.
4. **Automatic ke.** ke = 1, 2, … is tried, scoring each by `err max + 8.5·10⁻¹⁰·l1` measured on 2¹⁴ points. The best score wins. The search stops after 6 values in a row without improvement, or as soon as the coefficients stop being finite in double precision. Larger ke smooths the extension but makes T_A, T_B and l1 grow, so the score balances approximation error against noise.

The page also refuses a result with l1 > 2²⁰⁰ (the series would not fit in the remaining modulus). On very wide positive domains it adds extra log-spaced evaluation points near L, where the uniform grid is coarse.

The heavy computation runs in a Web Worker, with a fallback to the main thread if blob workers are blocked. Results and Hermite polynomials are cached, so going back to a previous setting is instant.

## CKKS level calculator

Opening **CKKS levels in OpenFHE** in the sidebar shows, below the statistics, when and where FE-FBT can be used in OpenFHE v1.6.1 compared with normal bootstrapping. The Fourier degree is the page's series degree. There is one row per operation: FE-FBT with that degree, and bootstrapping with 1 and 2 iterations.

| Column | Meaning |
| --- | --- |
| **Minimum effective input levels** | The levels the input must still have: what the operation spends before ModRaise. |
| **Last callable level, scale degree 1 / 2** | The deepest level at which the operation can be called. A ciphertext with scale degree 2 (not yet rescaled) needs one more level. |
| **Effective levels in output** | The levels left after the operation. |
| **Callable now** | Shown when *Check a ciphertext* is on: whether a ciphertext at the given level and scale degree can be passed in. |

"Effective levels" are the multiplications a ciphertext still allows. With a chain of depth D a ciphertext at level l has D − l of them, or D + 1 − l with FLEXIBLEAUTOEXT (one extra tower). If an operation would leave no effective level, its row says it is not feasible with this chain and gives the minimum D.

Inputs: the chain depth D, the number of slots (it caps the level budgets at log₂ slots, as OpenFHE does), the scaling technique (FLEXIBLEAUTO, FLEXIBLEAUTOEXT, FIXEDAUTO or FIXEDMANUAL; composite scaling is not modeled), the secret key distribution, the size of the first modulus q₀, and the level budgets {CoeffsToSlots, SlotsToCoeffs} of FE-FBT and of bootstrapping.

The model, with b = {CtS, StC} and ext = 1 for FLEXIBLEAUTOEXT (0 otherwise):

- `GetFEFBTDepth = CtS + StC + exp + R + (PS(d) − 1)`, where PS(d) is OpenFHE's `GetDepthByDegree` and exp + R is the complex-exponential table plus its double-angle squarings: 6 + 9 = 15 for UNIFORM_TERNARY, 6 + 4 = 10 for SPARSE_ENCAPSULATED (Hamming weight 32), 7 + 3 = 10 for SPARSE_TERNARY and for SPARSE_ENCAPSULATED with q₀ > 60 bits (Hamming weight 64).
- `GetBootstrapDepth = CtS + StC + 10` for sparse secrets, `+ 14` for UNIFORM_TERNARY.
- FE-FBT: minimum effective input StC + 1 + ext, last level D − StC − 1, output D − (GetFEFBTDepth − StC). Below degree 5 Paterson–Stockmeyer is not used and the output has one level less than GetFEFBTDepth predicts. The page's degrees start at 5.
- Bootstrapping: minimum effective input 1, last level D − 1 + ext, output D − GetBootstrapDepth for 1 iteration and one level less for 2.
- FE-FBT also requires FirstModSize = ScalingModSize + 1. On the 64-bit build ScalingModSize < 60, so q₀ > 60 bits is only possible with composite scaling or the 128-bit build. Neither is modeled here, and the 128-bit build does not support FE-FBT. Since both sparse tables cost 10 levels, the q₀ choice does not change the numbers.

### Checking the model against OpenFHE

`tools/levelprobe` runs the real operations on OpenFHE v1.6.1. For each input level and scale degree it calls FE-FBT or bootstrapping, checks the decrypted result, and counts the effective levels of the input and the output by multiplying by 1 until decryption fails.

```
git clone --depth 1 --branch v1.6.1 https://github.com/openfheorg/openfhe-development.git openfhe
cd openfhe && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_UNITTESTS=OFF -DBUILD_BENCHMARKS=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=$PWD/../install
make -j && make install && cd ../..
cmake -S tools/levelprobe -B tools/levelprobe/build && cmake --build tools/levelprobe/build
python3 tools/levelprobe/grid.py main     # one probe at a time, N = 2^10; writes build/grid-main.json
node tools/levelprobe/check.js tools/levelprobe/build/grid-main.json
```

The grid covers every scaling technique and secret distribution, budgets {1, 1} and {3, 2} (also {2, 2} and {4, 4}), 2⁵ and 2⁹ slots, the degree transitions of `GetDepthByDegree` from 1 to 1024, and 1 and 2 bootstrapping iterations. `tools/levelprobe/results/` holds the committed run, and `node tools/levelprobe/check.js` compares the page's `levelRows` with it. All 108 configurations that OpenFHE accepts match. OpenFHE refuses the other 3, which have q₀ = 61 bits.

## Custom functions

Type any expression in `x` in the function box. Enter applies it, and typing also applies it after a short pause. If an expression matches a preset, the preset is selected.

- **Operators:** `+ - * / ^` (or `**`) and parentheses. `-x^2` means −(x²), and `^` is right-associative. Juxtaposition is multiplication: `2x`, `3(x + 1)`, `x sin(x)`.
- **Constants:** `pi` (or `π`), `e`.
- **Functions:** `sin cos tan asin acos atan sinh cosh tanh asinh acosh atanh exp log ln log2 log10 sqrt abs erf erfc Phi sigmoid softplus relu gelu silu`, and the two-argument `pow(a, b) min(a, b) max(a, b)`. `Phi` is the standard normal CDF, and `gelu(x) = x·Phi(x)`.

How it works:

- The expression is parsed into a syntax tree, with a fixed whitelist of names. The tree is compiled to plain JavaScript for double-precision evaluation. User text is never evaluated directly.
- The derivatives needed for the Hermite extension come from Taylor-mode automatic differentiation on the same tree, in decimal.js at 110 digits. They are exact up to that precision, not finite differences. For the original built-in functions they match the hand-derived formulas to about 10⁻¹⁰⁸.

Requirements and error messages:

- **f must be smooth at L and U.** If it isn't, e.g. `log(x)` on a symmetric domain or `abs(x)` with an endpoint at 0, the page explains which endpoint fails.
- **f must be finite in double precision on the whole domain.** A pole (`1/x` on a symmetric domain) or an overflow (`exp(x)` for k ≥ 10) is reported with the x where it happens.
- **Kinks inside the domain** (`relu`, `abs`) are accepted. The series simply converges more slowly there.

## Files

- `index.html`: the whole application (HTML, CSS and JavaScript).
- `tools/levelprobe/`: the OpenFHE harness that checks the level calculator (`levelprobe.cpp`, `grid.py`, `check.js`, and the measured `results/`).
