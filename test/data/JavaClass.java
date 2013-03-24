/*
 * Copyright (C) 2013 Red Hat, Inc.
 * Written by Florian Weimer <fweimer@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.redhat.symboldb.test;

/**
 * Test for the class parser.
 */
public final class JavaClass extends Thread implements Runnable, AutoCloseable {
    public static final byte BYTE = 125;
    public static final short SHORT = 1250;
    public static final int INT = 125000;
    public static final long LONG = 125000000000L;
    public static final float FLOAT = 0.25f;
    public static final double DOUBLE = 9.094947017729282e-13;
    public static final String STRING = "\u0000a";

    public void run() {
    }

    public void close() {
    }

    static public void main(String[] args) throws Exception {
    }

    @Override
    public String toString() {
	return Byte.toString((byte) 126)
	    + Short.toString((short) 1260)
	    + Integer.toString(12600)
	    + Long.toString(126000000000L)
	    + Float.toString(0.75f)
	    + Double.toString(2.7284841053187847e-12)
	    + "\u0000b";
    }
}
