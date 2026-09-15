// Uses the signature implementation of the revision pinned in linux-update-sdk.json.
#include <vector>
#include "signing/signaturevalidator.h"
#include <algorithm>
#include <iostream>
int main(int argc, char** argv) {
    if (argc!=3) { std::cerr << "Usage: verify-appimage IMAGE SIGNING_FINGERPRINT\n"; return 2; }
    try {
        appimage::update::UpdatableAppImage image(argv[1]);
        if (image.readSignature().empty() || image.readSigningKey().empty())
            throw std::runtime_error("Unsigned AppImage");
        appimage::update::signing::SignatureValidator validator;
        auto result=validator.validate(image);
        auto keys=result.keyFingerprints();
        if (result.type()!=appimage::update::signing::SignatureValidationResult::ResultType::SUCCESS ||
            std::find(keys.begin(),keys.end(),argv[2])==keys.end())
            throw std::runtime_error("Invalid signature or unexpected signing key");
        std::cout << "Verified " << argv[2] << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
