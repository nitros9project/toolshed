# Bash completion for the `os9` ToolShed command.
# Source from ~/.bashrc or install in a bash_completion.d directory.
# Values shown as option prefixes must be attached (e.g. -b=8192).

_os9()
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
            COMPREPLY=( $(compgen -W "attr cmp copy dcheck del deldir dir dsave dump format free fstat gen id ident list makdir modbust padrom rename reveal" -- "$cur") )
        fi
        return 0
    fi

    case "$cmd" in
        attr) opts="-q -e -w -r -s -p -n -np" ;;
        cmp) opts="" ;;
        copy) opts="-b= -l -o= -a= -r" ;;
        dcheck) opts="-s -b -p" ;;
        del) opts="" ;;
        deldir) opts="-q" ;;
        dir) opts="-a -e -r" ;;
        dsave) opts="-b= -e -l -r -x=" ;;
        dump) opts="-a -b -c -e -t -h -l -z" ;;
        format) opts="-bs -c -e -f -k -n -q -4 -9 -sa -sd -dd -ss -ds -t -st -sz -i -dr -l" ;;
        free) opts="" ;;
        fstat) opts="" ;;
        gen) opts="-b= -c -d -e -t= -l" ;;
        id) opts="" ;;
        ident) opts="-s" ;;
        list) opts="" ;;
        makdir) opts="" ;;
        modbust) opts="" ;;
        padrom) opts="-b -c" ;;
        rename) opts="" ;;
        reveal) opts="-b -d" ;;
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

complete -o filenames -F _os9 os9
