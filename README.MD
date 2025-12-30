Forge
Production-Grade C Package Manager + HTTP Framework

Pure C. Zero dependencies. Real systems engineering.

Forge is a production-ready C99 ecosystem that includes:

- A fast package manager
- A low-level HTTP framework
- A fully tested build pipeline
- A real web server demo

No JavaScript. No CMake. No magic.
Just real C systems programming.

==================================================
QUICK START (Windows / MSYS2)
==================================================

Clone + Build (1 command)

git clone git@github.com:rsubrata383-debug/Forge.git
cd forge
make clean && make all

---

## RUN WEB SERVER

./build/user-app.exe
Open browser: http://localhost:8080

---

## PACKAGE MANAGER

./build/forge-pm.exe install pkg@1.0.0
./build/forge-pm.exe list
./build/forge-pm.exe remove pkg@1.0.0

==================================================
PRODUCTION COMMANDS
==================================================

make clean - Clean build artifacts
make framework - Build libforge.a (HTTP framework)
make pm - Build forge-pm.exe
make app - Build user-app.exe
make test - Run unit tests (3/3 PASS)
make integration - Run integration tests (2/2 PASS)

==================================================
PROJECT STRUCTURE
==================================================

forge/
├── Makefile - One-command build system
├── pm-tool/ - Package manager core
├── framework-core/ - HTTP framework (libforge.a)
├── user-app/ - Real web server demo
├── tests/ - 5/5 tests PASS
└── .gitignore - Clean repository

==================================================
REAL-WORLD USAGE
==================================================

Build application
make app

Run application (auto-installs dependencies)
./build/user-app.exe

Manage packages
./build/forge-pm.exe install pkg@1.0.0

==================================================
WHY USE FORGE?
==================================================

- Pure C99 (No JavaScript, No CMake)
- Zero dependencies (Fully self-contained)
- Production-ready (-O2 + FORTIFY_SOURCE)
- Fully tested (100% pass rate)
- Cross-platform (Windows / Linux / macOS)

==================================================
CONTRIBUTING
==================================================

make test
make integration
git push

==================================================
LICENSE
==================================================

MIT

==================================================
AUTHOR
==================================================

Made By Subrata Help of AI

C Systems Programming Master
Building real tools, not tutorials.

Star the repo if you believe in pure C power.
Forge v1.0 — production systems start here.
