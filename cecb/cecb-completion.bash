# Bash completion for the `cecb` ToolShed command (CECB File Tools Executive)
# Source this file (e.g. from ~/.bashrc) or drop it in
# /etc/bash_completion.d/ or /usr/local/etc/bash_completion.d/

_cecb()
{
    local cur prev cmd
    COMPREPLY=()

    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Global options
    local global_opts="
        -r
        -t
        -n
        -s
        -z
    "

    # Subcommands
    local commands="
        bulkerase
        copy
        dir
        dumpblock
        fstat
        leader
    "

    #
    # Find the subcommand (first non-option argument).
    #
    cmd=
    for ((i=1; i<COMP_CWORD; i++)); do
        case "${COMP_WORDS[i]}" in
            -*)
                ;;
            *)
                cmd="${COMP_WORDS[i]}"
                break
                ;;
        esac
    done

    #
    # No subcommand yet.
    #
    if [[ -z "$cmd" ]]; then
        if [[ "$cur" == -* ]]; then
            COMPREPLY=( $(compgen -W "$global_opts" -- "$cur") )
        else
            COMPREPLY=( $(compgen -W "$commands" -- "$cur") )
        fi
        return
    fi

    case "$cmd" in
        bulkerase)
            case "$prev" in
                -s)
                    COMPREPLY=( $(compgen -W "11025 22050 44100 48000 96000" -- "$cur") )
                    return
                    ;;
                -b)
                    COMPREPLY=( $(compgen -W "8 16 24 32" -- "$cur") )
                    return
                    ;;
            esac

            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "
                    -s
                    -b
                    -l
                " -- "$cur") )
            else
                COMPREPLY=( $(compgen -f -- "$cur") )
            fi
            ;;

        copy)
            case "$prev" in
                -d|-e)
                    return
                    ;;
            esac

            if [[ "$cur" == -* ]]; then
                COMPREPLY=( $(compgen -W "
                    -0
                    -1
                    -2
                    -3
                    -a
                    -b
                    -g
                    -n
                    -d
                    -e
                    -l
                    -t
                    -k
                    -s
                    -f
                    -c
                    -p
                " -- "$cur") )
            else
                COMPREPLY=( $(compgen -f -- "$cur") )
            fi
            ;;

        dir|fstat|leader|dumpblock)
            if [[ "$cur" == -* ]]; then
                [[ "$cmd" == dumpblock ]] &&
                    COMPREPLY=( $(compgen -W "-h" -- "$cur") )
            else
                COMPREPLY=( $(compgen -f -- "$cur") )
            fi
            ;;
    esac
}

complete -F _cecb cecb
