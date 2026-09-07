// Port of signing-request src/identity-proof.ts
#pragma once

#include <dwarfkit/antelope/chain/authority.hpp>
#include <dwarfkit/signing_request/signing_request.hpp>

namespace dwarfkit {

struct IdentityProof {
    DK_STRUCT("identity_proof")
    ChainId chainId;
    Name scope;
    TimePointSec expiration;
    PermissionLevel signer;
    Signature signature;
    DK_FIELDS(chainId, scope, expiration, signer, signature)

    // From an EOSIO authorization header string: "EOSIO <base64payload>".
    static Result<IdentityProof> fromString(std::string_view value);
    static Result<IdentityProof> from(std::string_view value) { return fromString(value); }
    static Result<IdentityProof> from(const json& value);
    // From a callback payload.
    static Result<IdentityProof> fromPayload(const json& payload,
                                             const SigningRequestEncodingOptions& options = {});

    // Transaction this proof resolves to.
    Result<Transaction> transaction() const;

    // Recover the public key that signed this proof.
    Result<PublicKey> recover() const;

    // Verify that given authority signed this proof; currentTime defaults to
    // the system clock.
    // Unreal's CoreMinimal.h defines verify() as a function-like macro in every
    // configuration, and parsing a declaration named verify with it active is a
    // compile error. Save, clear and restore the macro around the declaration;
    // push_macro/pop_macro is supported by MSVC, GCC and Clang and is a no-op when
    // nothing named verify is defined.
#pragma push_macro("verify")
#undef verify
    Result<bool> verify(const Authority& auth,
                        std::optional<TimePointSec> currentTime = std::nullopt) const;
#pragma pop_macro("verify")

    // Encode the proof to an EOSIO auth header string.
    std::string toString() const;
};

}  // namespace dwarfkit
