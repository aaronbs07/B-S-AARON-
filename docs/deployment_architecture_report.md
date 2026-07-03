# Deployment Architecture Report

## Deployment Model
Kumari Engine implements a modular multi-platform deployment pipeline. Staged asset packages, binary executables, and dependencies are packed and deployed target-by-target.

## Platform Support
1. **Windows**: Fully operational deployment (EXE, DLLs, resource packs).
2. **Android**: Gradle-based project structure with JNI setup and assets packed inside JNI layout.
3. **Linux / macOS / iOS**: Preparatory structure templates with configuration and platform-specific packaging files.
