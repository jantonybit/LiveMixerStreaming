# LiveMixer Streaming

Mesa de som virtual com ASIO e WASAPI, efeitos, roteamento e captura por aplicativo para
gravação e transmissão no Windows. **Gratuito**, sem anúncios, sem compras internas e sem
telemetria.

- Microsoft Store: https://apps.microsoft.com/detail/9P7KCRJB09VB
- Política de privacidade (PT-BR): https://jantonybit.github.io/LiveMixerStreaming/privacy-policy.html
- Política de privacidade (EN): https://jantonybit.github.io/LiveMixerStreaming/privacy-policy.en.html
- Store ID: `9P7KCRJB09VB` · Publicador: Jantonybit

## Código-fonte

Este repositório contém o **código-fonte completo correspondente** à versão `1.0.0.0`
distribuída na Microsoft Store (pacote `LiveMixerStreaming_1.0.0.0_x64.msix`), conforme
exigido pela licença GPLv3:

- `src/` — código do aplicativo (motor de áudio, DSP, mixer, MIDI, UI e janela Win32);
- `ThirdParty/asio_sdk/` — SDK ASIO da Steinberg (opção GPLv3), usado para baixa latência;
- `ThirdParty/imgui/` — Dear ImGui (MIT), núcleo e backends Win32/DX11 efetivamente
  compilados no executável;
- `CMakeLists.txt`, `scripts/` — arquivos de build;
- `docs/` — manuais PT-BR/EN;
- `LICENSE.md`, `GPL-3.0.txt`, `THIRD_PARTY_NOTICES.md` — licenças e avisos.

> **ASIO is a registered trademark of Steinberg Media Technologies GmbH.**
> Os drivers ASIO de fabricantes de hardware **não** são redistribuídos neste repositório;
> o usuário instala o driver da própria interface de áudio.

## Como compilar

Requisitos: Windows 10 build 20348+ ou Windows 11, CMake 3.20+, Ninja e MinGW-w64 (ou MSVC).
O projeto baixa/usa as ferramentas em `ThirdParty\` e não instala nada no sistema.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\bootstrap.ps1
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

O executável `CentralDeAudio.exe` e os manuais são gerados em `bin\`.

## Licença

Copyright (c) 2026 Jantonybit.

Este projeto é distribuído sob a **GNU General Public License v3.0**, por causa da opção
open source do SDK ASIO. Você pode redistribuí-lo e modificá-lo nos termos da GPLv3:
https://www.gnu.org/licenses/gpl-3.0.html

O programa é distribuído sem qualquer garantia. Veja `LICENSE.md` e
`THIRD_PARTY_NOTICES.md` para os detalhes e as licenças dos componentes incluídos.
