[title]: <> (Shell Over Reticulum)
## Shell Over Reticulum

The `rnsh` program lets you establish fully interactive remote shell sessions over Reticulum. It also allows you to pipe any program to or from a remote system, and is similar to how the ``ssh`` program works.

### Local Installation

If you do not have access to the Internet, or would prefer to install `rnsh` directly from this RNode, you can use the following instructions.

- If you do not have an Internet connection while installing make sure to install the [Reticulum](./s_rns.html) package first
- Download the [{PKG_BASE_rnsh}]({ASSET_PATH}{PKG_rnsh}) package from this RNode and unzip it
- Install it with the command `pip install ./{PKG_NAME_rnsh}`
- Verify the installed `rnsh` version by running `rnsh --version`

### Online Installation

If you are connected to the Internet, you can try to install the latest version of `rnsh` via the `pip` package manager.

- Install `rnsh` by running the command `pip install rnsh`
- Verify the installed `rnsh` version by running `rnsh --version`
