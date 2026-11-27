// levelprobe: measures, on the real OpenFHE library, the level accounting of
//   fefbt  EvalFEFuncBootstrap with a Fourier series of a given degree
//   bs1    EvalBootstrap, one iteration
//   bs2    EvalBootstrap, two iterations
// For every input level and noise scale degree (1 or 2) it tries the operation, checks the decrypted
// result, and counts how many plaintext multiplications the input and the output still allow
// ("effective levels"). The web page's level calculator is checked against this output.
//
// usage: levelprobe <mode> <st> <skd> <depth> <logN> <logSlots> <cts> <stc> <fourierDeg> <sms> <fms> [cdeg]
//   st:  FLEXIBLEAUTO | FLEXIBLEAUTOEXT | FIXEDAUTO | FIXEDMANUAL
//   skd: UNIFORM_TERNARY | SPARSE_TERNARY | SPARSE_ENCAPSULATED
#include <cmath>
#include <complex>
#include <iostream>
#include <string>
#include <vector>

#include "openfhe.h"
#include "scheme/ckksrns/ckksrns-fhe.h"

using namespace lbcrypto;

static ScalingTechnique parseST(const std::string& s) {
    if (s == "FLEXIBLEAUTO") return FLEXIBLEAUTO;
    if (s == "FLEXIBLEAUTOEXT") return FLEXIBLEAUTOEXT;
    if (s == "FIXEDAUTO") return FIXEDAUTO;
    if (s == "FIXEDMANUAL") return FIXEDMANUAL;
    throw std::runtime_error("bad st " + s);
}
static SecretKeyDist parseSKD(const std::string& s) {
    if (s == "UNIFORM_TERNARY") return UNIFORM_TERNARY;
    if (s == "SPARSE_TERNARY") return SPARSE_TERNARY;
    if (s == "SPARSE_ENCAPSULATED") return SPARSE_ENCAPSULATED;
    throw std::runtime_error("bad skd " + s);
}

int main(int argc, char** argv) {
    if (argc < 12) {
        std::cerr << "usage: levelprobe <mode> <st> <skd> <depth> <logN> <logSlots> <cts> <stc> <fourierDeg> <sms> <fms>\n";
        return 2;
    }
    const std::string mode = argv[1];
    const auto st = parseST(argv[2]);
    const auto skd = parseSKD(argv[3]);
    const uint32_t depth = std::stoul(argv[4]);
    const uint32_t logN = std::stoul(argv[5]);
    const uint32_t logSlots = std::stoul(argv[6]);
    const std::vector<uint32_t> budget = {static_cast<uint32_t>(std::stoul(argv[7])),
                                          static_cast<uint32_t>(std::stoul(argv[8]))};
    const uint32_t fdeg = std::stoul(argv[9]);
    const uint32_t sms = std::stoul(argv[10]);
    const uint32_t fms = std::stoul(argv[11]);
    const uint32_t slots = 1u << logSlots;
    const bool fefbt = (mode == "fefbt");

    // Fourier series 2 Re(c0 + c1 z + ... + c_d z^d), z = e^{i pi m}: c1 = 1/4 gives cos(pi m)/2; a tiny c_d
    // fixes the degree without changing the value.
    std::vector<std::complex<double>> coeffs(fdeg + 1, 0.0);
    if (fefbt) {
        coeffs[1] = 0.25;
        coeffs[fdeg] += std::complex<double>(1e-9, 0);
    }

    CCParams<CryptoContextCKKSRNS> p;
    p.SetSecretKeyDist(skd);
    p.SetSecurityLevel(HEStd_NotSet);
    p.SetRingDim(1u << logN);
    p.SetNumLargeDigits(3);
    p.SetKeySwitchTechnique(HYBRID);
    p.SetScalingModSize(sms);
    p.SetFirstModSize(fms);
    p.SetScalingTechnique(st);
    p.SetBatchSize(slots);
    p.SetMultiplicativeDepth(depth);
    if (fefbt) p.SetCKKSDataType(COMPLEX);

    CryptoContext<DCRTPoly> cc;
    try {
        cc = GenCryptoContext(p);
    } catch (const std::exception& e) {
        std::cout << "ERROR context " << e.what() << "\n";
        return 1;
    }
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);
    cc->Enable(FHE);
    try {
        if (fefbt)
            cc->EvalFEFuncBootstrapSetup(budget, {0, 0}, slots);
        else
            cc->EvalBootstrapSetup(budget, {0, 0}, slots);
    } catch (const std::exception& e) {
        std::cout << "ERROR setup " << e.what() << "\n";
        return 1;
    }
    auto kp = cc->KeyGen();
    cc->EvalMultKeyGen(kp.secretKey);
    cc->EvalBootstrapKeyGen(kp.secretKey, slots);

    const uint32_t towers = cc->GetCryptoParameters()->GetElementParams()->GetParams().size();
    std::cout << "INFO towers " << towers << " depth " << depth;
    if (fefbt) std::cout << " GetFEFBTDepth " << FHECKKSRNS::GetFEFBTDepth(budget, coeffs, skd, fms);
    std::cout << " GetBootstrapDepth " << FHECKKSRNS::GetBootstrapDepth(budget, skd) << "\n";

    std::vector<double> x(slots);
    for (uint32_t i = 0; i < slots; ++i) x[i] = -0.5 + (i + 0.5) / slots;
    std::vector<double> expect(slots);
    for (uint32_t i = 0; i < slots; ++i)
        expect[i] = fefbt ? 0.5 * std::cos(M_PI * x[i]) + 2e-9 * std::cos(M_PI * fdeg * x[i]) : x[i];

    auto maxErr = [&](const Ciphertext<DCRTPoly>& ct, const std::vector<double>& ref, double scale) {
        Plaintext pt;
        cc->Decrypt(kp.secretKey, ct, &pt);
        pt->SetLength(slots);
        auto v = pt->GetRealPackedValue();
        double e = 0;
        for (uint32_t i = 0; i < slots; ++i) e = std::max(e, std::abs(v[i] - scale * ref[i]));
        return e;
    };
    // number of successive multiplications by 1 (each one level) that still decrypt correctly
    auto effective = [&](Ciphertext<DCRTPoly> ct, const std::vector<double>& ref, double tol) {
        int n = 0;
        for (; n < 200; ++n) {
            try {
                auto c2 = cc->EvalMult(ct, 1.0);
                if (st == FIXEDMANUAL) cc->ModReduceInPlace(c2);
                if (!(maxErr(c2, ref, 1.0) < tol)) break;
                ct = c2;
            } catch (const std::exception&) {
                break;
            }
        }
        return n;
    };

    for (uint32_t sdeg = 1; sdeg <= 2; ++sdeg) {
        // top-down: stop once two consecutive levels succeed (the output does not depend on the input level)
        int okRun = 0;
        for (int lvlI = depth; lvlI >= 0 && okRun < 2; --lvlI) {
            const uint32_t lvl = lvlI;
            Ciphertext<DCRTPoly> ct;
            try {
                auto pt = cc->MakeCKKSPackedPlaintext(x, 1, lvl, nullptr, slots);
                ct = cc->Encrypt(kp.publicKey, pt);
                if (sdeg == 2) ct = cc->EvalMult(ct, 1.0);
            } catch (const std::exception&) {
                continue;
            }
            if (ct->GetNoiseScaleDeg() != sdeg) continue;
            int effIn = effective(ct, x, 1e-3);
            std::string status = "ok";
            int outLevel = -1, outDeg = -1, effOut = -1;
            double err = -1;
            try {
                Ciphertext<DCRTPoly> out;
                if (fefbt)
                    out = cc->EvalFEFuncBootstrap(ct, coeffs);
                else
                    out = cc->EvalBootstrap(ct, mode == "bs2" ? 2 : 1, mode == "bs2" ? 8 : 0);
                outLevel = out->GetLevel();
                outDeg = out->GetNoiseScaleDeg();
                err = maxErr(out, expect, 1.0);
                if (!(err < 1e-2)) status = "wrong";
                else effOut = effective(out, expect, 1e-2);
            } catch (const std::exception& e) {
                status = "throw";
            }
            okRun = (status == "ok") ? okRun + 1 : 0;
            std::cout << "ROW sdeg " << sdeg << " level " << lvl << " effIn " << effIn << " " << status
                      << " outLevel " << outLevel << " outDeg " << outDeg << " effOut " << effOut << " err " << err
                      << "\n";
        }
    }
    return 0;
}
