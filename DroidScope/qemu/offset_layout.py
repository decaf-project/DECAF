#!/usr/bin/python

import re
import sys
import getopt

DX = DY = PX = PY = KX = KY = 0

_RE_LINE = re.compile("^\s*(?P<keyword>[\w-]+)\s+{\s*$")
_RE_XY = re.compile("^(?P<start>\s*)(?P<xy>[x|y]\s+)(?P<num>\d+)(?P<end>\s*)$")

def main():
  ParseArgs()
  ParseInput()

def Usage():
  print >>sys.stderr, """
  Usage: %s --dx offset-x-display --dy offset-y-display --px offset-x-phone-buttons --py offset-y-phone-buttons --kx offset-x-keyboard --ky offset-y-keyboard < layout > layout2.

  Unspecified offsets default to 0 (unchanged).
  Reads from stdin, outputs to stdout.
  Phone buttons: soft-left/top/righ/bottom, home, dpad, dial, power, etc.
  Keyboard is the soft keyboard.

  If your shell doesn't let you use negative integers, use _ for minus sign,
  i.e. --dx _40 --dy _42 for <-40,-42).
  """ % (sys.argv[0])
  sys.exit(1)

def ParseArgs():
  global DX, DY, PX, PY, KX, KY
  try:
    options, args = getopt.getopt(sys.argv[1:], "", ["dx=", "dy=", "px=", "py=", "kx=", "ky="])
    for opt, value in options:
      if opt in ["--dx"]:
        DX = int(value.replace("_", "-"))
      elif opt in ["--dy"]:
        DY = int(value.replace("_", "-"))
      elif opt in ["--px"]:
        PX = int(value.replace("_", "-"))
      elif opt in ["--py"]:
        PY = int(value.replace("_", "-"))
      elif opt in ["--kx"]:
        KX = int(value.replace("_", "-"))
      elif opt in ["--ky"]:
        KY = int(value.replace("_", "-"))
      else:
        Usage()
  except getopt.error, msg:
    Usage()

def ParseInput():
  global DX, DY, PX, PY, KX, KY

  PHONE = [	"soft-left", "home", "back", "dpad-up", "dpad-down", "dpad-left", "dpad-right", "dpad-center", "phone-dial", "phone-hangup", "power", "volume-up", "volume-down" ]
  KEYBOARD = [   "DEL", "CAP", "CAP2", "PERIOD", "ENTER", "ALT", "SYM", "AT", "SPACE", "SLASH", "COMMA", "ALT2" ]

  mode = None
  while True:
    line = sys.stdin.readline()
    if not line:
      return
    m_line = _RE_LINE.match(line)
    if m_line:
      keyword = m_line.group("keyword")
      if keyword in ["display", "button"]:
        mode = keyword
        is_phone = False
        is_keyboard = False
        print >>sys.stderr, "Mode:", mode
      else:
        if mode == "button" and "{" in line:
          is_phone = keyword in PHONE
          is_keyboard = (len(keyword) == 1 and keyword.isalnum())
          if not is_keyboard:
            is_keyboard = keyword in KEYBOARD
    elif "}" in line:
      is_phone = False
      is_keyboard = False
      if mode == "display":
        mode = None
    else:
      m_xy = _RE_XY.match(line)
      if m_xy:
        x = 0
        y = 0
        if mode == "display":
          x = DX
          y = DY
        elif mode == "button" and is_phone:
          x = PX
          y = PY
        elif mode == "button" and is_keyboard:
          x = KX
          y = KY
        if x or y:
          d = m_xy.groupdict()
          n = int(d["num"])
          if d["xy"].startswith("x"):
            n += x
          else:
            n += y
          d["num"] = n
          line = "%(start)s%(xy)s%(num)s%(end)s" % d
    sys.stdout.write(line)




if __name__ == "__main__":
  main()
