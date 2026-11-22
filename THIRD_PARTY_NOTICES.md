# Avisos de terceiros — LiveMixer Streaming

Este arquivo acompanha o pacote `LiveMixerStreaming_1.0.0.0_x64.msix`.

## Código do aplicativo

Copyright (c) 2026 Jantonybit. Licenciado sob GNU General Public License v3.0.
O texto integral da GPLv3 acompanha o pacote no arquivo `docs/GPL-3.0.txt` e também está
disponível em https://www.gnu.org/licenses/gpl-3.0.html
Código-fonte completo correspondente: https://github.com/Jantonybit/LiveMixerStreaming

## Componentes incluídos no executável

| Componente | Licença | Observação |
|---|---|---|
| Steinberg ASIO SDK | GPLv3 (opção de código aberto) | (c) 2025 Steinberg Media Technologies GmbH. **ASIO is a registered trademark of Steinberg Media Technologies GmbH.** |
| Dear ImGui | MIT | Copyright (c) 2014-2026 Omar Cornut e contribuidores. Núcleo e backends `imgui_impl_win32` / `imgui_impl_dx11`. |

Ferramentas de build (CMake, Ninja, MinGW-w64/GCC) são programas separados, usados apenas
para compilar; seus fontes são obtidos pelos respectivos projetos e não fazem parte do
executável distribuído.

Drivers ASIO de fabricantes de hardware **não** são redistribuídos neste produto.

## Onde obter o código-fonte

O código-fonte completo e os scripts de compilação da versão `1.0.0.0` estão disponíveis
publicamente em:

https://github.com/Jantonybit/LiveMixerStreaming

A oferta é mantida por pelo menos três anos após a distribuição do pacote, nos termos da
GPLv3, seção 6.
