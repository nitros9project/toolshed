# Bash completion for the `dgndos` ToolShed command.
# Source from ~/.bashrc or install in a bash_completion.d directory.
# Values shown as option prefixes must be attached (e.g. -b=8192).

_dgndos()
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
            COMPREPLY=( $(compgen -W "-h -?" -- "$cur") )
        else
            COMPREPLY=( $(compgen -W "attr copy dir dsave dskinit dump free fstat kill list rename" -- "$cur") )
        fi
        return 0
    fi

    case "$cmd" in
        attr) opts="-p -u" ;;
        copy) opts="-l -r -t" ;;
        dir) opts="" ;;
        dsave) opts="-e -l -r -t -x=" ;;
        dskinit) opts="-t -s -f" ;;
        dump) opts="-a -b -c -e -t -h -l -z" ;;
        free) opts="" ;;
        fstat) opts="" ;;
        kill) opts="" ;;
        list) opts="-t" ;;
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

complete -o filenames -F _dgndos dgndos
