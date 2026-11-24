# FE-FBT Validator

An interactive, single-page tool to explore the Fourier-extension approximation used by FE-FBT: how a degree-*d* Fourier series, built on a period-2 Hermite extension of *f*, approximates *f* on a domain [L, U], and what that costs in terms of coefficient size and estimated CKKS noise.

## Running it

Open `main.html` in a modern browser. There is no build step and no server.

The page loads [decimal.js](https://github.com/MikeMcl/decimal.js) and [Plotly](https://plotly.com/javascript/) from cdnjs, and its fonts from Google Fonts, so it needs an internet connection the first time.

## Controls

| Control | Meaning |
| --- | --- |
| **Function** | A preset (GELU, sin, cos, exp, 1/√x, 1/x, sigmoid, tanh) or any expression in `x`; see [Custom functions](#custom-functions). |
| **Domain** | *Symmetric* [−2ᵏ, 2ᵏ] or *Positive* [2⁻ᵏ, 2ᵏ], with k = 0…15. |
| **Series degree** | d = 2⁴…2¹⁵. |
| **Hermite order** | The extension order ke, chosen automatically or set by hand (1…40). |
| **x axis** | Linear, or logarithmic (positive domains only). |
| **Show the extension** | Also plots the extension p(s) and the series on [U, U + W]. |
| **Error panel** | Absolute error, relative error, or bits of precision. |

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

- `main.html`: the whole application (HTML, CSS and JavaScript).
