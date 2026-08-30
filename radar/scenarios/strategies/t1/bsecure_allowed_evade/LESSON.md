# bsecure_allowed_evade — BSecureAllowed Trust Chain Evasion

Family: Evasion. Tiers: T1-T2.

## Battlefield
Red: Loads a module that can read CS2 memory. To avoid file trust checks,
     the module must pass BSecureAllowed() validation.
Blue: CS2's client.dll exports BSecureAllowed() which checks a module's
     file signature against specific whitelist criteria.

## Real AC Context
CS2's BSecureAllowed function validates:
1. The module is signed by a trusted certificate
2. The signature is valid and not revoked
3. The PE timestamps are consistent with the signing date
4. The module is not on the blocklist

If a module fails BSecureAllowed, it's flagged and reported (Message 159).

## Technique
Red can:
- Module stomp: overwrite a legitimate module that already passed
- Manual map: no module entry at all (but this has other scars)
- Timestamp clone: copy timestamps from a legitimate module
- Use a signed rootkit (BYOVD) that passes trust checks

## Lesson
Unsigned modules are immediately flagged. Even signed modules are checked
against revocation lists. Module stomping is detectable by size/checksum
changes.
