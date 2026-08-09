# Startup script for arm target

define flash_target
    target extended-remote localhost:3333
    monitor reset init
    monitor reset halt

    file "C:/projects/poke_vault/build/poke_vault_ctlr/poke_vault_ctlr.elf"
    monitor flash write_image erase "C:/projects/poke_vault/build/poke_vault_ctlr/poke_vault_ctlr.elf"

    monitor reset halt
    b main
end

flash_target