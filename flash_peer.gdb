# Startup script for arm target

define flash_target
    target extended-remote localhost:3333
    monitor reset init
    monitor reset halt

    file "C:/projects/poke_vault/build/poke_vault_peer/poke_vault_peer.elf"
    monitor flash write_image erase "C:/projects/poke_vault/build/poke_vault_peer/poke_vault_peer.elf"

    monitor reset halt
    b main
end

flash_target
