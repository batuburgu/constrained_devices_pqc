#include "kem.hpp"
#include <oqs/oqs.h>
#include <stdexcept>
#include <utility>

namespace pqc {

namespace {
// Initializes liboqs once, before first use, and cleans up at program exit.
struct OqsRuntime {
    OqsRuntime() { OQS_init(); }
    ~OqsRuntime() { OQS_destroy(); }
};
void EnsureInit() { static OqsRuntime runtime; }
}  // namespace

SecretBuffer::~SecretBuffer() {
    if (!data_.empty()) OQS_MEM_cleanse(data_.data(), data_.size());
}

SecretBuffer& SecretBuffer::operator=(SecretBuffer&& obj) noexcept {
    if (this != &obj) {
        if (!data_.empty()) OQS_MEM_cleanse(data_.data(), data_.size());
        data_ = std::move(obj.data_);
    }
    return *this;
}

void KEM::Deleter::operator()(OQS_KEM* obj) const { OQS_KEM_free(obj); }

KEM::KEM(const std::string& algorithm) {
    EnsureInit();
    kem_.reset(OQS_KEM_new(algorithm.c_str()));
    if (!kem_) {
        throw std::invalid_argument("KEM algorithm not available: " + algorithm);
    }
}

KEM::~KEM() = default;
KEM::KEM(KEM&&) noexcept = default;
KEM& KEM::operator=(KEM&&) noexcept = default;

/* Produce a key pair (public and private keys) */
KEM::KeyPair KEM::GenerateKeyPair() const {
    KeyPair keyPair{std::vector<uint8_t>(kem_->length_public_key), SecretBuffer(kem_->length_secret_key)};
    
    if (OQS_KEM_keypair(kem_.get(), keyPair.publicKey.data(), keyPair.privateKey.data()) != OQS_SUCCESS) {
        throw std::runtime_error("OQS_KEM_keypair failed");
    }
    return keyPair;
}
/* Produces a cipher text from the plain text with the publicKey provided */
KEM::Encapsulation KEM::Encapsulate(const std::vector<uint8_t>& publicKey) const {
    if (publicKey.size() != kem_->length_public_key) {
        throw std::invalid_argument("Encapsulate: wrong public key size");
    }

    Encapsulation enc{std::vector<uint8_t>(kem_->length_ciphertext), SecretBuffer(kem_->length_shared_secret)};
    if (OQS_KEM_encaps(kem_.get(), enc.cipherText.data(), enc.plainText.data(), publicKey.data()) != OQS_SUCCESS) {
        throw std::runtime_error("OQS_KEM_encaps failed");
    }
    return enc;
}

/* Extracts the secret from the cipher text */
SecretBuffer KEM::Decapsulate(const std::vector<uint8_t>& cipherText, const SecretBuffer& secretKey) const {
    if (cipherText.size() != kem_->length_ciphertext) {
        throw std::invalid_argument("Decapsulate: wrong ciphertext size");
    }
    if (secretKey.size() != kem_->length_secret_key) {
        throw std::invalid_argument("Decapsulate: wrong secret key size");
    }

    SecretBuffer ss(kem_->length_shared_secret);
    if (OQS_KEM_decaps(kem_.get(), ss.data(), cipherText.data(), secretKey.data()) != OQS_SUCCESS) {
        throw std::runtime_error("OQS_KEM_decaps failed");
    }
    return ss;
}

std::string KEM::GetName() const { return kem_->method_name; }
size_t KEM::GetPublicKeySize() const { return kem_->length_public_key; }
size_t KEM::GetSecretKeySize() const { return kem_->length_secret_key; }
size_t KEM::GetCiphertextSize() const { return kem_->length_ciphertext; }
size_t KEM::GetSharedSecretSize() const { return kem_->length_shared_secret; }
int KEM::GetNistLevel() const { return kem_->claimed_nist_level; }

std::vector<std::string> KEM::GetEnabledAlgorithms() {
    EnsureInit();
    std::vector<std::string> out;
    for (int i = 0; i < OQS_KEM_alg_count(); ++i) {
        const char* id = OQS_KEM_alg_identifier(i);
        if (OQS_KEM_alg_is_enabled(id)) out.emplace_back(id);
    }
    return out;
}

}  // namespace pqc