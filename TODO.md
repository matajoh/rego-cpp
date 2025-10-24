- [x] Add json builtins
- [x] Add stubs for remaining stdlib
    - [x] crypto
    - [x] glob
    - [x] graphql
    - [x] http
    - [x] net
    - [x] aws
    - [x] io.jwt
- [x] Modify test harness to skip tests whose builtins are not available
- [x] Modify tests to run the entire OPA directory (with the skipping)
    - Mention fixing the binary format (all builtins were being omitted from bundles)
- [x] Add Output
    - API change (get_string/get_raw_string)
- [x] VM timeout
- [ ] Builtin whitelist/blacklist
    - Do this with builtin factories. The builtin loaders can now (optionally) register
      factories instead of full builtins. 
    - Try replacing the Decl nodes with function pointers. It will be a bit of a
      heavy refactor, but the result will be that the nodes get generated lazily.
      Do a timing test with one of the larger sets to see if it has an impact.
- [ ] Selective builtin loading
- [ ] Release
    - [ ] bump version
    - [ ] Release notes
