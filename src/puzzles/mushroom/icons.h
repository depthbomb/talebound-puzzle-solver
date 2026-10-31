#pragma once

// Keep these embedded so DPI changes can redraw them without loading files.
// The glow uses translucent strokes that wxWidgets can render without SVG filters.
inline constexpr char MushroomThornsSvg[] = R"svg(
        <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
            <g fill="none" stroke="#80ff32" stroke-linecap="round">
                <path d="M6 6C1 11 3 18 7 20Q12 23 17 20C21 18 23 11 18 6"
                    stroke-width="5" stroke-opacity="0.09"/>
                <path d="M6 6C1 11 3 18 7 20Q12 23 17 20C21 18 23 11 18 6"
                    stroke-width="3.5" stroke-opacity="0.18"/>
            </g>
            <path d="M10 13L9 21Q12 22.5 15 21L14 13Z" fill="#f4dfb3" stroke="#785337"/>
            <path d="M4 13C6 10 9 4 12 2.5C15 4 18 10 20 13Q12 16 4 13Z"
                fill="#c44b3b" stroke="#713b2d"/>
            <g fill="#fff1d5">
                <circle cx="9" cy="10" r="1.6"/><circle cx="15" cy="11" r="1.6"/>
                <circle cx="12" cy="6.5" r="1.1"/>
            </g>
            <path d="M6 6C1 11 3 18 7 20Q12 23 17 20C21 18 23 11 18 6" fill="none"
                stroke="#45632c" stroke-width="1.5" stroke-linecap="round"/>
            <path d="M4 9L3 4L7 7ZM3 14L1 10L5 12ZM5 19L1.5 18L4 16Z
                M8 21L7 17L11 21ZM20 9L21 4L17 7ZM21 14L23 10L19 12Z
                M19 19L22.5 18L20 16ZM16 21L17 17L13 21Z"
                fill="#45632c" stroke="#80ff32" stroke-width="0.7" stroke-opacity="0.65"/>
        </svg>)svg";

inline constexpr char MushroomSvg[] = R"svg(
        <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
            <path d="M10 13L9 21Q12 22.5 15 21L14 13Z" fill="#f4dfb3" stroke="#785337"/>
            <path d="M4 13C6 10 9 4 12 2.5C15 4 18 10 20 13Q12 16 4 13Z"
                fill="#c44b3b" stroke="#713b2d"/>
            <g fill="#fff1d5">
                <circle cx="9" cy="10" r="1.6"/><circle cx="15" cy="11" r="1.6"/>
                <circle cx="12" cy="6.5" r="1.1"/>
            </g>
        </svg>)svg";
