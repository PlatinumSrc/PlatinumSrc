package org.psrc.psrc;
import org.libsdl.app.SDLActivity;

public class psrc extends SDLActivity {
    protected String getMainFunction() {
        return "main";
    }
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "psrc"
        };
    }
}
