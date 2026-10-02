# components/gui

**Not part of the build.** Excluded in the root `CMakeLists.txt` via
`set(EXCLUDE_COMPONENTS "gui")`, with this reasoning in that file's
own comment:

> components/gui is a leftover skeleton directory from the original
> project archive. Its CMakeLists.txt references components that
> don't exist yet ("system"), so ESP-IDF's automatic component
> discovery (which picks up every directory under components/,
> whether or not main requires it) fails to configure.

That comment also says "excluded here until gui is actually
implemented per `docs/BUILD_PLAN.md`" -- as of this README,
`docs/BUILD_PLAN.md` doesn't mention `gui` at all, so even that
stated reason for keeping it around no longer points anywhere.

## What's actually here

Just `CMakeLists.txt`. It lists six `SRCS` that don't exist
(`screen_manager.cpp`, `boot_screen.cpp`, `quick_screen.cpp`,
`unlock_screen.cpp`, `main_menu_screen.cpp`, `accounts_screen.cpp`) and
`REQUIRES` a `system` component that was never built under that name
-- this directory was never more than its build file, from an early
design pass before the project settled on its current structure.

## Where the real UI actually lives

**`components/ui`** -- 21 LVGL screens (lock screen, main menu, vault
list, account view, settings, ...), built out over the course of this
project and what every screenshot/test referenced elsewhere in this
repo's history actually refers to. `gui`'s own file names
(`boot_screen`, `unlock_screen`, `main_menu_screen`, `accounts_screen`)
map loosely onto `ui`'s real screens (`lock_screen`, `main_menu`,
`accounts_screen`, `vault_list_screen`, ...), suggesting `gui` was an
earlier naming/structure attempt for the same idea, abandoned before
any of the six files were actually written.

## Recommendation

Safe to delete entirely -- nothing references this directory's content
(only its presence on disk, which is what the `EXCLUDE_COMPONENTS`
workaround exists to neutralize), and the real implementation has
long since moved to `components/ui`. Not deleted as part of writing
this README specifically because that's a slightly bigger call than
documenting what's here, left for a deliberate follow-up commit
instead.
