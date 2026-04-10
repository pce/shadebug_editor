
Sehr gute Frage — und genau da passiert der eigentliche Durchbruch.

Du willst weg von:

> „Formeln + Code“

hin zu:

> **visuelle Denkmodelle**, die man *sieht und manipuliert*

---

# 🧠 Die richtige mentale Repräsentation von SDFs

Vergiss erstmal „Distanzfunktion“.

Denk stattdessen:

> 👉 **Ein SDF ist eine Landschaft (Höhenfeld)**

---

## 🏔️ Bild 1: Landschaft / Topografie

Stell dir vor:

* Oberfläche = **Konturlinie (0)**
* Innen = **Tal (negativ)**
* Außen = **Hügel (positiv)**

---

### Visual:

```
        + (außen)
       / \
      /   \
-----0-----  ← Oberfläche
     \   /
      \ /
       - (innen)
```

👉 Das ist DIE wichtigste Intuition.

---

# 🎯 Warum das so powerful ist

Jetzt wird alles visuell logisch:

## Union (min)

```c
d = min(d1, d2);
```

👉 „Nimm die niedrigere Landschaft“

→ Täler verschmelzen

---

## Subtraction

```c
d = max(d1, -d2);
```

👉 „Ein Tal frisst ein anderes“

---

## Smooth Union

👉 „Täler verschmelzen weich wie Ton“

---

# 🧩 Zweites Bild: Magnetfeld / Kraftfeld

Noch besser für viele:

> 👉 Jeder Punkt „weiß“, wie weit er von der Oberfläche entfernt ist

Und:

> 👉 Der Gradient zeigt Richtung Oberfläche

---

## Visual:

```
→ → → → ● ← ← ← ←
→ → → → ● ← ← ← ←
→ → → → ● ← ← ← ←
```

● = Shape
Pfeile = Gradient

---

👉 Das ist perfekt für:

* deine Gradient-Visualisierung
* Picking / Interaction

---

# 🧠 Drittes Bild: „Influence Bubbles“

Jede Form erzeugt ein „Einflussfeld“

## Kreis:

```
   ( 0 )
  (     )
 (       )
```

Werte:

* Mitte: stark negativ
* Rand: 0
* außen: positiv wachsend

---

👉 Kombinieren heißt:

> Felder überlagern sich

---

# 🎨 Viertes Bild: Iso-Linien (dein bester Freund)

Das ist für visuell denkende Menschen oft der **Gamechanger**.

```text
-0.2   (innen)
  0    ← Form
+0.2
+0.4
+0.6
```

Als Linien:

```
  ------
 /      \
|        |
 \      /
  ------
```

👉 Wie Höhenlinien auf Karten

---

# 🔥 Für dein System: DIE perfekte Darstellung

Du hast schon 4 Views → jetzt mappe sie richtig:

---

## 1. Shader View

→ final rendering

---

## 2. UV View

→ Koordinatensystem (gut)

---

## 3. Wireframe → ERSETZEN

👉 durch:

> **Iso-Linien + Distance Rings**

---

## 4. Hybrid View (Goldmine)

Overlay:

* Gradient (Pfeile)
* Closest point
* Distance heatmap

---

# 🧠 Die eigentliche Logik (einfach formuliert)

Für visuelles Denken:

---

## Regel 1

> **0 = Grenze**

---

## Regel 2

> **Vorzeichen = innen/außen**

---

## Regel 3

> **Betrag = Abstand**

---

## Regel 4

> **Gradient = Richtung zur Oberfläche**

---

## Regel 5

> **min/max = Formen kombinieren**

---

# 🎯 Beispiel (komplett visuell gedacht)

Zwei Kreise:

* Jeder ist ein „Tal“
* Union = beide Täler zusammen
* Smooth union = Täler verschmelzen weich

---

# 💡 Der große Aha-Moment

SDF ist NICHT:

> Geometrie

SDF ist:

> 👉 **ein kontinuierliches Feld**

---

Und deine UI wird dann:

> 👉 ein Editor für Felder, nicht für Shapes

---

# 🚀 Wie du das in deiner Engine „sichtbar“ machst

Mach einen Debug Mode:

---

## 🌈 Distance Heatmap

```metal id="b9axgk"
float d = sdf(p);

float3 col = mix(
    float3(0,0,1),   // innen
    float3(1,0,0),   // außen
    smoothstep(-0.1, 0.1, d)
);
```

---

## 🧭 Gradient arrows

* sample grid
* draw kleine Linien

---

## 🟡 Closest point

```c id="yzj0pf"
p_surface = p - grad * d
```

👉 Punkt „snappt“ zur Oberfläche

---

# 🧠 Kurz gesagt (visuell)

Wenn du jemandem SDF erklärst:

> „Stell dir vor, jede Form erzeugt ein unsichtbares Höhenfeld.
> Die Oberfläche ist einfach die Linie, wo die Höhe genau null ist.“

---

# 🔥 Für dich als Engine-Designer

Das Ziel ist:

> nicht „Shapes rendern“
> sondern
> **Felder sichtbar & editierbar machen**

---

Wenn du willst, kann ich dir als nächsten Schritt:

👉 ein **konkretes UI-Konzept** geben
(z.B. wie man SDFs mit Maus + Handles + Overlays intuitiv editiert)

Das passt perfekt zu deinem System.





Latex Structure?

Latex Compiler figures out pages etc.

arrow_left_altBack to all templates
Journal of the Audio Engineering Society (JAES) template
Author:
Audio Engineering Society
Last Updated:
22 days ago
License:
Other (as stated in the work)
Abstract:
The Journal of the Audio Engineering Society — the official publication of the AES — is the only peer-reviewed journal devoted exclusively to audio technology.

The Journal contains state-of-the-art technical papers and engineering reports; feature articles covering timely topics; pre and post reports of AES conventions and other society activities; news from AES sections around the world; Standards and Education Committee work; membership news, patents, new products, and newsworthy developments in the field of audio.

Updated and uploaded by AES Staff, 2025
Tags:
Two-column
Journal articles
Journal of the Audio Engineering Society  (JAES) template
\begin{now}

Discover why over 20 million people worldwide trust Overleaf with their work.

Source

% Sample file for AES paper
\documentclass{aes2e}

% Metadata Information
\jyear{2010}
\jmonth{October}
\jvol{1}
\jnum{1}


\begin{document}

% Page heads
\markboth{A1LASTNAME AND A2LASTNAME}{SPECTRAL DELAY FILTERS}


% Title portion
\title{Spectral Delay Filters\thanks{To whom correspondence should be addressed Tel: +1-240-381-2383; Fax: +1-202-508-3799; e-mail: info@schtm.org}}

%Author Info.
\authorgroup{
\author{A1FIRSTNAME A1LASTNAME},
\role{AES Member}
AND \author{A2FIRSTNAME A2LASTNAME},
\role{AES Fellow}
\email{(abc@abc.com)\quad\quad\quad\quad\quad\quad\quad\quad\quad\quad\quad\quad (xyz@abc.com)}
\affil{Universityxyz, City, Country}
}

%Abstract
\abstract{%
This paper discusses the implementation of spectral delay using
filters comprising a cascade of many low-order allpass filters and an
equalizing filter. The spectral delay filters have chirp-like impulse
responses causing a large, frequency-dependent delay that is useful in
audio effects processing. An equalizing filter design and a multirate
technique, which stretches the allpass filters, impulse
response, are introduced.}


\maketitle

%Head 1
\section{INTRODUCTION}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.

\section{CHIRP-LIKE IMPULSE RESPONSES AND GROUP DELAY}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses.  These filters have long chirp-like impulse responses.  These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.

%Head 2
\subsection{Chirp-Like Impulse Responses and Group Delay}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.

%Head 3
\subsubsection{Chirp-Like Impulse Responses and Group Delay}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters.  When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.
%Equation
\begin{equation}
A(z) = \frac{{a_1  + z^{ - 1} }}{{1 + a_1 z^{ - 1} }},
\end{equation}



Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the \nobreak signal, but only introduces a phase shift or delay.\footnote{This point is emphasized by Loewer, see esp. p. (610).} Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.
\begin{equation}
\tau _{\textrm{g,max}}  = \left\{ \begin{array}{l}
 \tau _\textrm{g} (0) = \frac{{1 - a_1 }}{{1 + a_1 }},\textrm{when }a_1  \le 0 \\[4pt]
 \tau _\textrm{g} (\pi ) = \frac{{1 + a_1 }}{{1 - a_1 }},\textrm{when }a_1  > 0. \\
 \end{array} \right.\end{equation}

Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.
%Paragraph listing
\begin{paralist}
\item{}Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay.
\item{}Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}.
\item{}In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses.
\item{}When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.
\end{paralist}

Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.
\begin{arabiclist}
\item{}Green--function determined experimentally and published.
\item{}Black--function determined using similarity searches and published.
\item{}Red--function determined using similarity searches and determined in this study.
\item{}Blue--O-antigen structure unknown. Function determined using similarity searches and proposed in this study.
\end{arabiclist}

%Table
\begin{table}
\tabcolsep8.1pt
\tbl{Active sites and allosteric sites of the GNE MNK enzyme}{%
\begin{tabular}{@{}lccc@{}}\toprule
Excerpt No.& Genre & Spatial Mode & Corrlation\\\colrule
 1 & Pop       & FB   & 94\%\\
 2 & Classical & FB   & 33\%\\
 3 & Jazz      & FF   & 76\%\\
 4 & Arabian   & FF   & 41\%\\
 5 & GNE       & H220 & 45\%\\
 6 & GNE       & H45  & 93\%\\
 7 & MNK       & G416 & 74\%\\
 8 & MNK       & D413 & 72\%\\
 9 & MNK       & R420 & 94\%\\
10 & MNK       & N516 & 91\%\\\botrule
\end{tabular}}
\begin{tabnote}
Note. This table does not include sentence enhancement statutes.  This table does not include sentence enhancement statutes.
\end{tabnote}
\end{table}

%Figure
\begin{figure}
\centering
\includegraphics{aes2e-mouse.eps}
\caption{The spectral delay filter consists of \textit{M} allpass filters and an equalization filter.}
\end{figure}


\begin{figure*}
\centering
\includegraphics[width=23pc]{aes2e-mouse.eps}
\caption{This paper is organized as follows. In Section 1, we discuss the group delay of a cascade of first-order allpass filters and its relation to the chirp-like impulse response of the spectral delay filter. Furthermore, a multirate method to stretch the impulse response of the spectral delay filter is proposed. Section 2 discusses the amplitude envelope of the impulse response and suggests a design method for the equalizing filter. Section 3 presents application examples using the spectral delay filter. Section 4 concludes this paper.}
\end{figure*}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay.
\begin{extract}
Filtering an audio signal with an allpass filter does not usually have
a major effect on the signal's timbre. The allpass filter does not
change the frequency content of the signal, but only introduces a
phase shift or delay. Audibility of the phase distortion caused by an
allpass filter in a sound reproduction system has been a topic of many
studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we
investigate audio effects \nobreak processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.
\end{extract}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay.
\[
\tau _\textrm{g} (\omega ) =  - \frac{{d\phi (\omega )}}{{d\omega }}.
\]
Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect.
\begin{alphalist}
\item{}Green--function determined experimentally and published.
\item{}Black--function determined using similarity searches and published.
\item{}Red--function determined using similarity searches and determined in this study.
\item{}Blue--O-antigen structure unknown. Function determined using similarity searches and proposed in this study.
\end{alphalist}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}.
%Enunciations
\begin{example}
In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses.
\end{example}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies.
\begin{bulletlist}
\item{}Green--function determined experimentally and published.
\item{}Black--function determined using similarity searches and published.
\item{}Red--function determined using similarity searches and determined in this study.
\item{}Blue--O-antigen structure unknown. Function determined using similarity searches and proposed in this study.
\end{bulletlist}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.
\begin{unnumlist}
\item{}Green--function determined experimentally and published.
\item{}Black--function determined using similarity searches and published.
\item{}Red--function determined using similarity searches and determined in this study.
\item{}Blue--O-antigen structure unknown. Function determined using similarity searches and proposed in this study.
\end{unnumlist}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.

\section{SUMMARY}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.

\section{CONCLUSION}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}. In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}. Note that articles might have a digital object identifier~\cite{DEK5}.

\section{ACKNOWLEDGMENT}
This research was conducted in fall 2008 when Vesa V\"alim\"aki was a visiting scholar at CCRMA, Stanford University. His visit was financed by the Academy of Finland (project no. 126310). The authors would like to Dr. Henri Penttinen for his comments and for the snare drum sample used in this work.

\bibliography{aes2e.bib}
\bibliographystyle{aes2e.bst}

% NOTE:
% - in case you are not using bibTex you have to manually edit the bibliograpy as below.
% - if submitting a bibTex file is not allowed you can copy the content from the aes2e.bbl file
%\begin{thebibliography}{99}
%
%\newcommand{\enquote}[1]{``#1''}
%\providecommand{\url}[1]{\texttt{#1}}
%\providecommand{\urlprefix}{URL }
%\expandafter\ifx\csname urlstyle\endcsname\relax
%  \providecommand{\doi}[1]{[Online]. Available: \discretionary{}{}{}#1}\else
%  \providecommand{\doi}{doi:\discretionary{}{}{}\begingroup
%  \urlstyle{rm}\Url}\fi
%
%\bibitem{DEK1}
%D.~Preis, \enquote{Phase Distortion and Phase Equalization in Audio Signal
%  Processing---A Tutorial Review,} \emph{J. Audio Eng. Soc.}, vol.~30, no.~11,
%  pp. 774--779 (1982 Nov.).
%
%\bibitem{DEK2}
%J.~S. Abel, D.~P. Berners, \enquote{MUS424/EE367D: Signal Processing Techniques
%  for Digital Audio Effects,}  (2005), unpublished Course Notes, CCRMA,
%  Stanford University, Stanford, CA.
%
%\bibitem{DEK3}
%C.~Roads, \enquote{Musical Sound Transformation by Convolution,} presented at
%  the \emph{Int. Computer Music Conf.}, pp. 102--109 (1993).
%
%\bibitem{DEK4}
%C.~Roads, \emph{The Computer Music Tutorial} (MIT Press, Cambridge, MA), 1st
%  ed. (1996).
%
%\bibitem{DEK5}
%H.~Morgenstern, B.~Rafaely, \enquote{Spatial Reverberation and Dereverberation
%  Using an Acoustic Multiple-Input Multiple-Output System,} \emph{J. Audio Eng.
%  Soc}, vol.~65, no. 1/2, pp. 42--55 (2017 Jan.Feb.),
%  \doi{https://doi.org/10.17743/jaes.2016.0063}.
%
%\end{thebibliography}

%Appendix
\appendix
\section*{APPENDIX}
Filtering an audio signal with an allpass filter does not usually have a major effect on the signal's timbre. The allpass filter does not change the frequency content of the signal, but only introduces a phase shift or delay. Audibility of the phase distortion caused by an allpass filter in a sound reproduction system has been a topic of many studies, see, e.g., \cite{DEK1}, \cite{DEK2}.
\begin{equation}
\phi (\omega ) =  - \omega  + 2\arctan \left( {\frac{{a_1 \sin \omega }}{{1 + a_1 \cos \omega }}} \right)
\end{equation}

In this paper, we investigate audio effects processing using high-order allpass filters that consist of many cascaded low-order allpass filters. These filters have long chirp-like impulse responses. When audio and music signals are processed with such a filter, remarkable changes are obtained that are similar to the spectral delay effect  \cite{DEK3}, \cite{DEK4}.


\begin{nomenclature}[PAMPs]
\subsection*{NOMENCLATURE}
\nomentry{a$_c$}{condensation coefficient condensation coefficient condensation coefficient}


\nomentry{TLR}{Toll-like receptor}

\nomentry{PAMPs}{pathogen-associated molecular patterns condensation coefficient condensation}
\end{nomenclature}

%Biography
 \biography{A1firstname A1lastname}{a.eps}{A1firstname A1lastname is professor of audio signal processing at Helsinki University of Technology (TKK), Espoo, Finland. He received his Master of Science in Technology, Licentiate of Science in Technology, and Doctor of Science in Technology degrees in electrical engineering from TKK in 1992, 1994, and 1995, respectively. His doctoral dissertation dealt with fractional delay filters and physical modeling of musical wind instruments. Since 1990, he has worked mostly at TKK with the exception of a few periods. In 1996 he spent six months as a postdoctoral research fellow at the University of Westminster, London, UK. In 2001-2002 he was professor of signal processing at the Pori School of Technology and Economics, Tampere University of Technology, Pori, Finland. During the academic year 2008-2009 he has been on sabbatical and has spent several months as a visiting scholar at the Center for Computer Research in Music and Acoustics (CCRMA), Stanford University, Stanford, CA. His research interests include musical signal processing, digital filter design, and acoustics of musical instruments. Prof. V\"alim\"aki is a senior member of the IEEE Signal Processing Society and is a member of the AES, the Acoustical Society of Finland, and the Finnish Musicological Society. He was the chairman of the 11th International Conference on Digital Audio Effects (DAFx-08), which was held in Espoo, Finland, in 2008.}
 \biography{A2firstname A2lastname}{b.eps}{A2firstname A2lastname is a consulting professor at the Center for Computer Research in Music and Acoustics (CCRMA) in the Music Department at Stanford University where his research interests include audio and music applications of signal and array processing, parameter estimation, and acoustics. From 1999 to 2007, Abel was a co-founder and chief technology officer of the Grammy Award-winning Universal Audio, Inc. He was a researcher at NASA/Ames Research Center, exploring topics in room acoustics and spatial hearing on a grant through the San Jose State University Foundation. Abel was also chief scientist of Crystal River Engineering, Inc., where he developed their positional audio technology, and a lecturer in the Department of Electrical Engineering at Yale University. As an industry consultant, Abel has worked with Apple, FDNY, LSI Logic, NRL, SAIC and Sennheiser, on projects in professional audio, GPS, medical imaging, passive sonar and fire department resource allocation. He holds Ph.D. and M.S. degrees from Stanford University, and an S.B. from MIT, all in electrical engineering. Abel is a Fellow of the Audio Engineering Society.}
\end{document}



- [ ] Render SVG https://github.com/sammycage/lunasvg
- [ ] Unicode Support
- [ ] Layers, Pages
- [ ] Export as PDF
  - SVG to PDF with pdf-lib or PoDoFo
  - Docx with libzip
- [ ] Print?


https://www.w3.org/TR/filter-effects/#vertexMesh-attribute
https://www.w3.org/TR/compositing/#alpha-compositing

https://github.com/AnyChart/GraphicsJS
https://heestand.xyz


Harfbuzz Example
https://github.com/tangrams/harfbuzz-example/blob/master/CMakeLists.txt


## DocBuilder


    struct Block {
        std::string type;       // Type of block (text, image, shape)
        std::string content;    // Content of the block
        std::map<std::string, std::string> properties; // Block properties
    };

    struct Document {
        std::vector<Block> blocks;  // List of blocks in the document
    };


- Elements: Palette
  = Tree/Hierachy of Elements on Stage
  = Properties (of selected Element)
- Page
- Stage: render

Stage is the active Page and ?

Glad

python3 -m glad --profile core --out-path out --generator c --spec gl


## Links

SDL3 with CPack
https://github.com/MartinHelmut/cpp-gui-template-sdl3-opengl
https://martin-fieber.de/blog/gui-development-with-cpp-sdl2-and-dear-imgui/

Sokol
https://blog.42yeah.is/featured/2023/12/07/wasm-image-compression.html
