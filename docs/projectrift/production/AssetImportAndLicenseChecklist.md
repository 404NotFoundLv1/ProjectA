# ProjectRift Asset Import and License Checklist

Before importing production art, audio, animation, font, or third-party material:

- Keep the original source file outside cooked content and record its storage location.
- Create an exact record in `Config/ProjectRift/AssetLicenseRegistry.json` for each imported asset. Folder coverage is only for reviewed Project-owned placeholder/data content.
- Record source, vendor/author, commercial-use permission, modification permission, attribution requirement/text, license document path, invoice path where applicable, reviewer, and review date.
- Unknown or missing commercial, modification, or attribution permission is blocking. The validator confirms declarations and evidence paths; legal approval remains a human responsibility.
- Use `T_`, `SM_`, `SK_`, `M_`, `MI_`, `S_`, `SC_`, `A_`, `AM_`, and `ABP_` naming as appropriate, then run `-run=PRProductionValidation`.

Never import models, music, fonts, or AI-generated assets whose commercial rights cannot be documented.
