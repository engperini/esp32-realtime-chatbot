# Handoff — API Realtime nova desconectando

Data: 2026-09-07
Branch de continuidade: `feat/realtime-chatbot-improvements`

## Estado publicado

- Commit base: `045e4c16ce68c1bc324c6b43c7cd9f3ad31982fa`
- Build ESP-IDF concluído com sucesso para ESP32-S3.
- A migração/configuração da API Realtime nova ainda **não está funcional em hardware**: a conexão WebSocket/API desconecta.
- Não considerar a integração validada até capturar e analisar o log serial de uma tentativa controlada de conexão.

## Próxima investigação

1. Conectar o dispositivo e capturar o log serial desde o boot até a desconexão.
2. Registrar o evento/erro específico do WebSocket ou TLS sem expor chave de API, senha Wi-Fi ou outros segredos.
3. Comparar URI, cabeçalhos e payload `session.update` com o protocolo Realtime atual.
4. Corrigir somente após planejar e obter autorização explícita.
5. Compilar, gravar no ESP32 somente com autorização e validar pelo log serial.

## Testes

- O teste `tests/realtime_tls_config_test.py` falha porque ainda espera `ws_cfg.buffer_size = 4096;`, enquanto a configuração atual usa `16384`.
- Por decisão do usuário, esse teste será revisado posteriormente; não corrigir agora.
