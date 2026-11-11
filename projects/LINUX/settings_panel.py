#!/usr/bin/python3

import os
from pathlib import Path
from tempfile import NamedTemporaryFile


DEFAULTS = (50, 40, 50, 10, 1, 1, 1, 5, 4)


def config_path():
    root = Path(os.environ.get("XDG_CONFIG_HOME", ""))
    if not root.is_absolute():
        root = Path.home() / ".config"
    return root / "ekk" / "painel-linux.conf"


def valid(values):
    return (len(values) == 9 and all(0 <= value <= 100 for value in values[:4])
            and sum(values[1:4]) == 100 and all(value in (0, 1) for value in values[4:7])
            and all(value in (1, 4, 5) for value in values[7:9]))


def load_settings(path):
    try:
        values = tuple(map(int, path.read_text().split()))
    except (OSError, ValueError):
        return DEFAULTS
    if len(values) == 4:
        values += DEFAULTS[4:]
    return values if valid(values) else DEFAULTS


def save_settings(path, values):
    if not valid(values):
        raise ValueError("Os percentuais precisam somar 100%.")
    path.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as stream:
        temp = Path(stream.name)
        try:
            stream.write(" ".join(map(str, values)) + "\n")
            stream.close()
            temp.replace(path)
        finally:
            temp.unlink(missing_ok=True)


def cs2_processes(proc=Path("/proc")):
    processes = []
    for entry in proc.iterdir():
        if not entry.name.isdecimal():
            continue
        try:
            if (entry / "comm").read_text().strip() not in {"cs2", "cs2.exe"}:
                continue
        except OSError:
            continue
        try:
            executable = os.readlink(entry / "exe")
        except OSError:
            executable = "Caminho indisponível"
        processes.append((int(entry.name), executable))
    return sorted(processes)


def main():
    try:
        import gi
        gi.require_version("Gtk", "4.0")
        from gi.repository import Gtk
    except (ImportError, ValueError):
        raise SystemExit("O painel requer Python 3 com PyGObject e GTK 4.")

    app = Gtk.Application(application_id="io.github.dmsylvio.EkkSettings")

    def activate(application):
        if application.get_active_window():
            application.get_active_window().present()
            return

        path = config_path()
        values = load_settings(path)
        strength = values[0]
        regions = values[1:4]
        window = Gtk.ApplicationWindow(application=application, title="EKK · Configurações Linux")
        window.set_default_size(410, -1)

        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        for edge in ("top", "bottom", "start", "end"):
            getattr(content, f"set_margin_{edge}")(20)
        window.set_child(content)

        content.append(Gtk.Label(label="Instância do CS2 para diagnóstico", xalign=0))
        process_row = Gtk.Box(spacing=8)
        process_list = Gtk.DropDown(hexpand=True)
        process_list.update_property([Gtk.AccessibleProperty.LABEL], ["Instância do CS2"])
        refresh = Gtk.Button(label="Atualizar")
        process_row.append(process_list)
        process_row.append(refresh)
        content.append(process_row)
        process_detail = Gtk.Label(xalign=0, wrap=True, selectable=True)
        content.append(process_detail)
        processes = []

        def show_process(*_):
            selected = process_list.get_selected()
            if selected < len(processes):
                pid, executable = processes[selected]
                process_detail.set_text(f"PID {pid} · {executable}")
            elif processes:
                process_detail.set_text("Selecione uma instância para ver o caminho.")
            else:
                process_detail.set_text("Nenhuma instância do CS2 encontrada.")

        def refresh_processes(*_):
            selected = process_list.get_selected()
            previous = processes[selected] if selected < len(processes) else None
            processes[:] = cs2_processes()
            labels = [f"CS2 · PID {pid}" for pid, _ in processes]
            process_list.set_model(Gtk.StringList.new(labels))
            if previous in processes:
                process_list.set_selected(processes.index(previous))
            elif len(processes) == 1:
                process_list.set_selected(0)
            else:
                process_list.set_selected(Gtk.INVALID_LIST_POSITION)
            process_list.set_sensitive(bool(processes))
            show_process()

        process_list.connect("notify::selected", show_process)
        refresh.connect("clicked", refresh_processes)
        refresh_processes()

        toggles = []
        for label, enabled in zip(("Wall", "Aim", "Trigger"), values[4:7]):
            toggle = Gtk.CheckButton(label=label)
            toggle.set_active(bool(enabled))
            content.append(toggle)
            toggles.append(toggle)

        buttons = []
        choices = (1, 4, 5)
        for label, selected in zip(("Segurar para Aim", "Segurar para Trigger"), values[7:9]):
            row = Gtk.Box(spacing=12)
            row.append(Gtk.Label(label=label, xalign=0, hexpand=True))
            selector = Gtk.DropDown.new_from_strings(["Mouse 1", "Mouse 4", "Mouse 5"])
            selector.set_selected(choices.index(selected))
            row.append(selector)
            content.append(row)
            buttons.append(selector)

        strength_label = Gtk.Label(label="Força da mira (%)", xalign=0)
        content.append(strength_label)
        slider = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
        slider.set_draw_value(True)
        slider.set_value(strength)
        slider.update_property([Gtk.AccessibleProperty.LABEL], ["Força da mira"])
        content.append(slider)

        fields = []
        for label, value in zip(("Cabeça (%)", "Tronco (%)", "Braços e pernas (%)"), regions):
            row = Gtk.Box(spacing=12)
            caption = Gtk.Label(label=label, xalign=0, hexpand=True)
            field = Gtk.SpinButton.new_with_range(0, 100, 1)
            field.set_numeric(True)
            field.set_value(value)
            field.update_property([Gtk.AccessibleProperty.LABEL], [label])
            row.append(caption)
            row.append(field)
            content.append(row)
            fields.append(field)

        status = Gtk.Label(xalign=0, wrap=True)
        content.append(status)
        save = Gtk.Button(label="Salvar", halign=Gtk.Align.END)
        save.add_css_class("suggested-action")
        content.append(save)

        def update_total(*_):
            total = sum(field.get_value_as_int() for field in fields)
            status.set_text(f"Soma: {total}%. Os percentuais precisam somar 100%.")
            save.set_sensitive(total == 100)

        def apply(_):
            values = (int(slider.get_value()), *(field.get_value_as_int() for field in fields),
                      *(int(toggle.get_active()) for toggle in toggles),
                      *(choices[button.get_selected()] for button in buttons))
            try:
                save_settings(path, values)
            except (OSError, ValueError) as error:
                status.set_text(f"Falha ao salvar: {error}")
                return
            status.set_text("Configurações salvas. O EKK em execução aplica as alterações automaticamente.")

        for field in fields:
            field.connect("value-changed", update_total)
        slider.connect("value-changed", update_total)
        save.connect("clicked", apply)
        update_total()
        window.present()

    app.connect("activate", activate)
    return app.run([])


if __name__ == "__main__":
    raise SystemExit(main())
