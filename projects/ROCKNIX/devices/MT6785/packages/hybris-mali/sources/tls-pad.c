// SPDX-License-Identifier: Apache-2.0
// LD_PRELOAD for libhybris apps. It claims tp+16..tp+63, the bionic TLS slots the Android GL
// driver writes, so those writes miss other libraries' thread-locals.
// Needs an executable without its own TLS.
__attribute__((tls_model("initial-exec"), aligned(16), used, visibility("default")))
__thread char hybris_tls_pad[64];
