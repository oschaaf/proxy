1.20.4 (June 9, 2022)
=====================
1.20.6 (June 16, 2022)
======================
1.20.8 (Pending)
================

Incompatible Behavior Changes
-----------------------------
*Changes that are expected to cause an incompatibility if applicable; deployment changes are likely required*

Minor Behavior Changes
----------------------
*Changes that may cause incompatibilities for some users, but should not for most*

Bug Fixes
---------
*Changes expected to improve the state of the world and are unlikely to have negative effects*

* decompression: fixed CVE-2022-29225 due to which decompressors can be zip bombed. Previously decompressors were susceptible to memory inflation in takes in which specially crafted payloads could cause a large amount of memory usage by Envoy. The max inflation payload size is now limited.  This change can be reverted via the ``envoy.reloadable_features.enable_compression_bomb_protection`` runtime flag.
* health_check: fixed CVE-2022-29224 which caused a segfault in GrpcHealthCheckerImpl. An attacker-controlled upstream server that is health checked using gRPC health checking can crash Envoy via a null pointer dereference in certain circumstances.
* oauth: fixed CVE-2022-29226 due to which oauth filter allows trivial bypass. The OAuth filter implementation does not include a mechanism for validating access tokens, so by design when the HMAC signed cookie is missing a full authentication flow should be triggered. However, the current implementation assumes that access tokens are always validated thus allowing access in the presence of any access token attached to the request.
* oauth: fixed CVE-2022-29228 due to which oauth filter calls continueDecoding() from within decodeHeaders(). The OAuth filter would try to invoke the remaining filters in the chain after emitting a local response, which triggers an ASSERT() in newer versions and corrupts memory on earlier versions.
* router: fixed CVE-2022-29227 which caused an internal redirect crash for requests with body/trailers. Envoy would previously crash in some cases when processing internal redirects for requests with bodies or trailers if the redirect prompts an Envoy-generated local reply.
* ci: fix disk space issue that have prevented publication.

Removed Config or Runtime
-------------------------

New Features
------------

Deprecated
----------
