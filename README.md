# Click Assist (mod de Geode)

## Compilar
1. Instala el Geode CLI y el SDK: https://docs.geode-sdk.org/getting-started/
2. `geode sdk install` y luego `geode sdk install-binaries`
3. En esta carpeta:
   ```
   geode build
   ```
   Se genera un `.geode` que puedes copiar a la carpeta `geode/mods` de tu juego.

## Sin instalar nada
Sube esta carpeta a un repositorio de GitHub: la accion de `.github/workflows/build.yml`
compila el mod para Windows, macOS y Android y deja el `.geode` en "Artifacts".

## Antes de compilar
- Cambia `id` y `developer` en `mod.json` (formato `tunombre.click-assist`).
