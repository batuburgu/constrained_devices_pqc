#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct OQS_KEM;

namespace pqc {

    class SecretBuffer {
        public:
        SecretBuffer() = default;
        explicit SecretBuffer(size_t size) : data_(size) {}
        ~SecretBuffer();
        SecretBuffer(SecretBuffer&& o) noexcept : data_(std::move(o.data_)) {}
        SecretBuffer& operator=(SecretBuffer&& obj) noexcept;
        SecretBuffer(const SecretBuffer&) = delete;
        SecretBuffer& operator=(const SecretBuffer&) = delete;

        uint8_t* data() { return data_.data(); }
        const uint8_t* data() const { return data_.data(); }
        size_t size() const { return data_.size(); }

        private:
            std::vector<uint8_t> data_;
    };

    class KEM {
        public:
            struct KeyPair {
                std::vector<uint8_t> publicKey;
                SecretBuffer privateKey;            
            };
            struct Encapsulation {
                std::vector<uint8_t> cipherText;
                SecretBuffer plainText;            
            };

            explicit KEM(const std::string& algorithm);
            ~KEM();
            KEM(KEM&&) noexcept;
            KEM& operator = (KEM&&) noexcept;
            KEM(const KEM&) = delete;
            KEM& operator=(const KEM&) = delete;
            
            KeyPair GenerateKeyPair() const;
            Encapsulation Encapsulate(const std::vector<uint8_t>& publicKey) const;
            SecretBuffer Decapsulate(const std::vector<uint8_t>& ciphertext, const SecretBuffer& secretKey) const;
            
            std::string GetName() const;
            size_t GetPublicKeySize() const;
            size_t GetSecretKeySize() const;
            size_t GetCiphertextSize() const;
            size_t GetSharedSecretSize() const;
            int GetNistLevel() const;

            static std::vector<std::string> GetEnabledAlgorithms();    
        
        private:
            struct Deleter { void operator()(OQS_KEM* k) const; };
            std::unique_ptr<OQS_KEM, Deleter> kem_;
    };
} // namespace pqc