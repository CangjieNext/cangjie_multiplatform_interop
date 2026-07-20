/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Huawei designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Huawei in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

package cangjie.interop.cangjie.emit;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

public final class IndentedString {
    private static final Object NEW_LINE = new Object();
    private static final Object INDENT_INC = new Object();
    private static final Object INDENT_DEC = new Object();

    private List<Object> events = new ArrayList<>(10);
    private String cache;
    private boolean finished;

    public IndentedString() {
    }

    public void append(IndentedString ts) {
        assert !finished;
        assert ts.finished;

        if (ts.events.isEmpty()) {
            return;
        }

        events.addAll(ts.events);
        cache = null;
    }

    public void append(String s) {
        assert s.indexOf('\r') == -1 : '"' + s + "\" should not contain CR, only LF";
        int i = 0;
        for (int nl; (nl = s.indexOf('\n', i)) != -1; i = nl + 1) {
            if (i != nl) {
                final var substr = s.substring(i, nl);
                events.add(substr);
            }
            events.add(NEW_LINE);
        }
        if (i < s.length()) {
            final var substr = s.substring(i);
            events.add(substr);
        }

        cache = null;
    }

    public void indentInc() {
        assert !finished;
        events.add(INDENT_INC);
    }

    public void indentDec() {
        assert !finished;
        events.add(INDENT_DEC);
    }

    public void newLine() {
        assert !finished;
        events.add(NEW_LINE);
    }

    public void finish() {
        assert !finished;
        this.events = Collections.unmodifiableList(events);
        this.finished = true;
    }

    public String text() {
        if (cache != null) {
            return cache;
        }

        final var sb = new StringBuilder();
        text(sb);
        return cache = sb.toString();
    }

    @Override
    public String toString() {
        return text();
    }

    private void text(StringBuilder sb) {
        int indent = 0;

        for (final var event : events) {
            if (event == NEW_LINE) {
                sb.append('\n');
            } else if (event == INDENT_INC) {
                ++indent;
            } else if (event == INDENT_DEC) {
                --indent;
            } else if (event instanceof String str) {
                if (!str.isEmpty()) {
                    appendIndented(sb, indent, str);
                }
            } else {
                assert false : Objects.toString(event);
            }
        }
    }

    private void appendIndented(StringBuilder sb, int indent, String str) {
        assert !str.isEmpty();
        assert str.indexOf('\n') == -1;

        if (!sb.isEmpty() && sb.charAt(sb.length() - 1) == '\n') {
            sb.append("    ".repeat(indent));
        }
        if (sb.length() > 2 && sb.substring(sb.length() - 2).equals("*/") && str.startsWith("/*")) {
            sb.delete(sb.length() - 2, sb.length());
            sb.append(str.substring(2));
        } else {
            sb.append(str);
        }
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) {
            return true;
        }
        if (obj == null || obj.getClass() != this.getClass()) {
            return false;
        }
        var that = (IndentedString) obj;
        return Objects.equals(this.events, that.events);
    }

    @Override
    public int hashCode() {
        return Objects.hash(events);
    }
}
