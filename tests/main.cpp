// Catch2 v3 provides main() via Catch2WithMain linkage; this file just
// hard-codes the LeakSanitizer suppressions so the ASan build is self-
// sufficient (test discovery / --list-tests no longer needs the
// LSAN_OPTIONS env var set externally).
//
// CPython, pybind11 and OpenUSD all leak at shutdown / from static
// initialisers in ways that aren't actionable from our code. Keeping the
// list synced with lsan.suppressions at the repo root.

extern "C" const char* __lsan_default_suppressions() {
    return
        // CPython + pybind11 — shutdown-order leaks.
        "leak:libpython\n"
        "leak:pybind11\n"
        "leak:Py_Finalize\n"
        "leak:_PyObject_GC_New\n"
        // OpenUSD — process-lifetime singletons / type registry.
        "leak:libusd_tf\n"
        "leak:libusd_trace\n"
        "leak:libusd_arch\n"
        "leak:libusd_sdf\n"
        "leak:Tf_RegistryManagerImpl\n"
        "leak:TfType\n"
        "leak:TfSingleton\n"
        "leak:TraceEventList\n"
        "leak:Sdf_IdentityRegistry\n"
        "leak:Sdf_ChangeManager\n";
}
