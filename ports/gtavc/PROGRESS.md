# GTAVC Progress

- Menu/front-end now renders correctly on the test device.
- The direct `DoSettings + RequestFrontEndShutDown` path was removed because it crashed the session.
- Current attempt uses menu request flags plus delayed controller pulses to start the game safely.
- Next observed blocker before this note: `level=0` with menu visible and stable.
- Latest family skip added: `wtn`, `kb_planter`.
- Latest exact skip added: `wtn_roadsigns01`.

