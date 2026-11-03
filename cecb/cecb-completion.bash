# Bash completion for the `cecb` ToolShed command.
# Source from ~/.bashrc or install in a bash_completion.d directory.
# Values shown as option prefixes must be attached (e.g. -b=8192).

_cecb()
{
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local cmd= opts= candidate i
    COMPREPLY=()

    # Executive options precede the first non-option word.
    for ((i=1; i<COMP_CWORD; i++)); do
        case "${COMP_WORDS[i]}" in
            -r|-t|-n|-s) ((i++)) ;;
            -*) ;;
            *) cmd="${COMP_WORDS[i]}"; break ;;
        esac
    done
    # CECB also accepts separate values for its executive options.
    if (( i > COMP_CWORD )); then
        return 0
    fi

    if [[ -z "$cmd" ]]; then
        if [[ "$cur" == -* ]]; then
            COMPREPLY=( $(compgen -W "-r -t -n -s -z -h -?" -- "$cur") )
        else
            COMPREPLY=( $(compgen -W "bulkerase copy dir dump dumpblock fstat leader list" -- "$cur") )
        fi
        return 0
    fi

    case "$cmd" in
        bulkerase) opts="-s -b -l" ;;
        copy) opts="-0 -1 -2 -3 -a -b -g -n -d -e -l -t -k -s -f -c -p" ;;
        dir) opts="" ;;
        dump) opts="-a -b -c -e -t -h -l -z" ;;
        dumpblock) opts="-a -b -e -c -t -l -h" ;;
        fstat) opts="" ;;
        leader) opts="" ;;
        list) opts="-t -s" ;;
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

complete -o filenames -F _cecb cecb
