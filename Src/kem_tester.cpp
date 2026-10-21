#include "kem.hpp"

#include <cstring>
#include <exception>
#include <iostream>

int main() {
    try {
        pqc::KEM kem("ML-KEM-768");

        auto keyPair = kem.GenerateKeyPair();                                   // receiver
        auto enc = kem.Encapsulate(keyPair.publicKey);                          // sender
        pqc::SecretBuffer ss = kem.Decapsulate(enc.cipherText, keyPair.privateKey);  // receiver

        bool ok = ss.size() == enc.sharedKey.size() &&
                  std::memcmp(ss.data(), enc.sharedKey.data(), ss.size()) == 0;

        std::cout << kem.GetName() << "\n"
                  << "  public key:    " << kem.GetPublicKeySize() << " bytes\n"
                  << "  ciphertext:    " << kem.GetCiphertextSize() << " bytes\n"
                  << "  shared secret: " << kem.GetSharedSecretSize() << " bytes\n"
                  << (ok ? "  secrets match\n" : "  MISMATCH\n");
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}