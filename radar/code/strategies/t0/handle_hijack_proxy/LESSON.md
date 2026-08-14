# 33_handle_hijack_proxy

**Theme:** Delivery residual where a *legitimate-looking process* owns `VmRead`
to the game (handle hijack / process proxy), and the cheat consumer never opens
its own handle — so naive "handles from cheat.exe" sensors miss the reader.

**Red:** Spawn proxy (`svchost.exe`, `looks_reputable`), open handle from proxy,
set `handle_proxy_*` scars; cheat process has zero game handles.

**Blue:** Multi-reason: reputable/`via_proxy` handle + `handle_proxy_active` +
consumer without direct handle → detect + ranked deny.

**Not:** Real kernel hijack or Themes injection — sim scars only.
