NAM Grid standalone app (nam-grid.exe): license and source
==========================================================

The NAM Grid plugins (CLAP and VST3) and the NAM Grid source code are MIT licensed.

The standalone app nam-grid.exe is distributed under the GNU General Public License,
version 3 or (at your option) any later version; see GPL-3.0.txt. It includes the
Steinberg ASIO SDK 2.3.4, which Steinberg licenses under GPL version 3 (see
ASIO_SDK-LICENSE.txt), so the app as a whole is covered by GPL version 3.

ASIO is a registered trademark of Steinberg Media Technologies GmbH.

Corresponding source
--------------------

- NAM Grid: https://github.com/Fannon/nam-grid, at the release tag that matches the
  app version shown in the Settings page. Every other component is pinned there as a
  git submodule or in scripts/setup-deps.py.
- Steinberg ASIO SDK 2.3.4:
  https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip
  (SHA-256 d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca)

Build it on Windows with "bash build.sh --app"; docs/build.md describes the
prerequisites. The other open-source components and their licenses are listed in
ThirdPartyNotices.txt and the notices folder.
