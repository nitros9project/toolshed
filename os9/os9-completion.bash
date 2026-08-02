# Bash completion for the `os9` ToolShed command (OS-9 File Tools Executive)
# Source this file (e.g. from ~/.bashrc) or drop it in
# /etc/bash_completion.d/ or /usr/local/etc/bash_completion.d/

_os9_commands="attr cmp copy dcheck del deldir dir dsave dump format free fstat gen id ident list makdir modbust padrom rdump rename"

_os9_opts_attr="-q -e -w -r -s -p -n -np"
_os9_opts_cmp=""
_os9_opts_copy="-b -l -o -a -r"
_os9_opts_dcheck="-s -b -p"
_os9_opts_del=""
_os9_opts_deldir="-q"
_os9_opts_dir="-a -e -r"
_os9_opts_dsave="-b -e -l -r"
_os9_opts_dump="-a -b -c -e -t -h -l -z"
_os9_opts_format="-bs -c -e -k -n -q -4 -9 -sa -sd -dd -ss -ds -t -st -sz -i -dr -l"
_os9_opts_free=""
_os9_opts_fstat=""
_os9_opts_gen="-b -c -d -e -t -l"
_os9_opts_id=""
_os9_opts_ident="-s"
_os9_opts_list=""
_os9_opts_makdir=""
_os9_opts_modbust=""
_os9_opts_padrom="-b -c"
_os9_opts_rdump="-g -r -o -a"
_os9_opts_rename=""

# Options that take a value directly attached, e.g. -b=size or -bs256.
# We still just offer them as tokens; bash completion doesn't do the
# rich "expects an argument" prompting that zsh's _arguments does.
_os9_takes_value_attr="-b -bs -c -n -st -sz -i -l -o -a"

_os9() {
    local cur prev words cword
    _init_completion 2>/dev/null || {
        # Fallback if bash-completion's _init_completion isn't available
        COMPREPLY=()
        cur="${COMP_WORDS[COMP_CWORD]}"
        prev="${COMP_WORDS[COMP_CWORD-1]}"
        words=("${COMP_WORDS[@]}")
        cword=$COMP_CWORD
    }

    local subcmd="${words[1]}"

    # No subcommand yet, or still typing it: complete subcommand names
    if [[ $cword -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "$_os9_commands" -- "$cur") )
        return 0
    fi

    # Make sure the subcommand is one we know; otherwise fall back to files
    if [[ ! " $_os9_commands " == *" $subcmd "* ]]; then
        COMPREPLY=( $(compgen -f -- "$cur") )
        return 0
    fi

    local opts_var="_os9_opts_${subcmd}"
    local opts="${!opts_var}"

    # Completing an option value for options that take one
    if [[ " $_os9_takes_value_attr " == *" $prev "* ]]; then
        COMPREPLY=()
        return 0
    fi

    if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
        return 0
    fi

    # Default: complete filenames. Note that OS-9 paths often look like
    # `image.dsk,path/inside/image` (host disk image + in-image path
    # separated by a comma) rather than a plain host filesystem
    # directory, so we don't restrict any subcommand to compgen -d.
    COMPREPLY=( $(compgen -f -- "$cur") )
}

complete -F _os9 os9
