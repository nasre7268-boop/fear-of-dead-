FEAR OF DEAD - Trial Version (PS3 homebrew, PSL1GHT)
====================================================
Original action-survival game: 3 waves + boss, axe combat, dodge roll,
score, title/death/trial-complete screens. Software-rendered 720p.

CONTROLS
  Left stick / D-Pad : move
  SQUARE             : axe swing
  CROSS              : dodge roll
  START              : start / restart

BUILD (Linux / WSL)
  1. Install ps3toolchain + PSL1GHT  (github.com/ps3dev/ps3toolchain)
  2. export PS3DEV=/usr/local/ps3dev
     export PSL1GHT=$PS3DEV
     export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin
  3. make pkg        ->  fear_of_dead.pkg
  4. Install the .pkg on a PS3 running CFW/HEN (Package Manager > Install).

TEST ON PC (no PS3 needed)
  gcc -O2 -o test source/test_host.c source/game.c -lm && ./test
  (runs headless and writes shot_*.ppm screenshots)

NOTES
  - game.c / game.h = all gameplay (verified compiling + running headless).
  - main_ps3.c      = PSL1GHT glue, NOT compiled here (no PS3 SDK available).
    If PSL1GHT API names differ in your version, fix there only.
  - If the Makefile fails, copy source/*.c into any PSL1GHT sample folder
    and use that sample's Makefile (change TITLE/APPID/CONTENTID).
  - Console video output must be 720p or higher.

BUILD WITHOUT INSTALLING ANYTHING (GitHub Actions)
  1. Create a new GitHub repo and upload this whole folder (keep .github/).
  2. Open the Actions tab > "Build PS3 PKG" > Run workflow.
  3. When it finishes, download the artifact "fear_of_dead_pkg" = the .pkg.
  If the build fails, send the log text to Claude to fix the Makefile.
