# Bash completion for the `decb` ToolShed command (DECB File Tools Executive)
# Source this file (e.g. from ~/.bashrc) or drop it in
# /etc/bash_completion.d/ or /usr/local/etc/bash_completion.d/

_decb()
{
    local cur prev words cword
    _init_completion || return

    local commands="
        attr
        binbust
        copy
        dir
        dsave
        dskini
        free
        fstat
        hdbconv
        kill
        list
        rename
    "

    # Complete command name
    if [[ $cword -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
        return
    fi

    local cmd="${words[1]}"

    case "$cmd" in
        attr)
            COMPREPLY=( $(compgen -W \
                "-0 -1 -2 -3 -a -b" -- "$cur") )
            ;;

        binbust)
            _filedir
            ;;

        copy)
            COMPREPLY=( $(compgen -W \
                "-0 -1 -2 -3 -a -b -l -r -t -c" -- "$cur") )
            if [[ "$cur" != -* ]]; then
                _filedir
            fi
            ;;

        dir|free|kill|fstat|list)
            if [[ "$cur" != -* ]]; then
                _filedir
            fi
            ;;

        dsave)
            COMPREPLY=( $(compgen -W \
                "-b= -e -l -t -r" -- "$cur") )
            if [[ "$cur" != -* ]]; then
                _filedir
            fi
            ;;

        dskini)
            COMPREPLY=( $(compgen -W \
                "-3 -4 -8 -h -n -s" -- "$cur") )
            if [[ "$cur" != -* ]]; then
                _filedir
            fi
            ;;

        hdbconv)
            COMPREPLY=( $(compgen -W \
                "-2 -5" -- "$cur") )
            if [[ "$cur" != -* ]]; then
                _filedir
            fi
            ;;

        rename)
            _filedir
            ;;

        *)
            COMPREPLY=( $(compgen -W "-g" -- "$cur") )
            ;;
    esac
}

complete -F _decb decb
