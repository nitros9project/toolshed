# Bash completion for the `decb` ToolShed command.
# Source from ~/.bashrc or install in a bash_completion.d directory.
# Values shown as option prefixes must be attached (e.g. -b=8192).

_decb()
{
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local cmd= opts= candidate i
    COMPREPLY=()

    # Executive options precede the first non-option word.
    for ((i=1; i<COMP_CWORD; i++)); do
        case "${COMP_WORDS[i]}" in
            -*) ;;
            *) cmd="${COMP_WORDS[i]}"; break ;;
        esac
    done
    if [[ -z "$cmd" ]]; then
        if [[ "$cur" == -* ]]; then
            COMPREPLY=( $(compgen -W "-g -h -?" -- "$cur") )
        else
            COMPREPLY=( $(compgen -W "attr binbust copy dir dsave dskini dump free fstat hdbconv kill list rename" -- "$cur") )
        fi
        return 0
    fi

    case "$cmd" in
        attr) opts="-0 -1 -2 -3 -a -b" ;;
        binbust) opts="" ;;
        copy) opts="-0 -1 -2 -3 -a -b -l -r -t -c" ;;
        dir) opts="" ;;
        dsave) opts="-b= -e -l -t -r -x=" ;;
        dskini) opts="-3 -4 -8 -f -h -n -s" ;;
        dump) opts="-a -b -c -e -t -h -l -z" ;;
        free) opts="" ;;
        fstat) opts="" ;;
        hdbconv) opts="-2 -5" ;;
        kill) opts="" ;;
        list) opts="-t -s" ;;
        rename) opts="" ;;
    esac
    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
    else
        # Preserve spaces in host filenames. Image-internal paths are not queried.
        while IFS= read -r candidate; do
            COMPREPLY+=("$candidate")
        done < <(compgen -f -- "$cur")
    fi
    return 0
}

complete -o filenames -F _decb decb
