# PHAVerLite

PHAVerLite (<a href="http://www-verimag.imag.fr/~frehse/phaver_web/index.html">PHAVer</a> +
<a href="https://github.com/ezaffanella/PPLite">PPLite</a>) is a variant of PHAVer,
a formal verification tool for computing reachability of hybrid systems.
<p>
The main difference with respect to PHAVer,
originally developed by <a href="https://sites.google.com/site/frehseg/">Goran Frehse</a>
and later included as a plugin in the <a href="http://spaceex.imag.fr/">SpaceEx</a> platform,
is the replacement of the Parma Polyhedra Library (<a href="https://www.bugseng.com/parma-polyhedra-library">PPL</a>)
with PPLite, a software library exploiting novel representations and algorithms for the manipulation of polyhedra.

<h3>Current version</h3>
<ul>

<li>
2024-04-12:
<a href="releases/phaverlite-0.7.tar.gz">PHAVerLite 0.7 can be downloaded</a>.
<br>
This is based on PPLite 0.12.
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.7_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 5.15.0-101.
</li>

<li>
<a href="#downloads">Links to all versions released</a>.
</li>

</ul>

<h3>Usage example</h3>
Try command <strong>phaverlite -v256001 osc_demo.pha</strong>.
<br>
File <a href="osc_demo.pha">osc_demo.pha</a> contains the hybrid model,
the configuration commands and (commented out) the graph utility command
needed to produce an image of the computed reachable set.


<h3>Building from sources</h3>
PHAVerLite is currently developed on a Linux system.
In order to build it from sources the following
dependencies need to be satisfied (see file README):
<ul>
<li>a C++ compiler supporting the 2017 standard
(e.g., <strong>g++</strong> or <strong>clang++</strong>);</li>
<li>reasonably recent versions of <strong>flex</strong>
and <strong>bison</strong>;</li>
<li>reasonably recent versions of numeric libraries
<strong>GMP</strong>, <strong>MPFR</strong> and <strong>FLINT</strong>;</li>
<li>a <em>specific</em> version of the PPLite library.</li>
</ul>
While not strictly a requirement, we suggest to install the
<strong>graphviz</strong> tools to allow graphical display of
the generated output.

<h3>Experimental evaluation</h3>
PHAVerLite took part to several editions of the ARCH-COMP friendly competition.

<h3>Support</h3>
If you need help for using PHAVerLite,
<a href="mailto:enea.zaffanella@unipr.it">ask me</a>.</em>
<p>
Here are <a href="#phaverlites-specification-language">a few notes on PHAVerLite's
specification language</a>.

<h3>Features</h3>
PHAVerLite currently provides a subset of the functionalities offered by PHAVer, focusing on automata where:
<ul>
<li>state variable are continuous;</li>
<li>each location invariant is a finite set of rational, convex,
NNC (not necessarily topologically closed) polyhedra;</li>
<li>each discrete transition between locations is a convex linear
predicate on pre/post values of the state variables;</li>
<li>the continuous dynamics at locations is modeled by
piecewise constant bounds on the derivatives of state variables.</li>
</ul>
Note that some of the PHAVer functionalities (e.g., the computation of simulation relations)
have been deliberately removed, whereas a few of the design and
implementation choices have been (or are being) reconsidered.
While being heavily based on the original PHAVer,
PHAVerLite source code is being rewritten in modern C++,
so as to simplify both maintenance and experimentation
with novel algorithms and design tradeoffs.
The developers should feel free to use language features made
available by the recent standards, provided these lead to
a simpler implementation or improve on code <em>readability</em>.

<h3>Development team</h3>
As said above, the starting point for the project was
the source code of PHAVer, developed by Goran Frehse.
The following people have contributed to the project:

<ul>
<li>Enea Zaffanella (main developer, supervisor)</li>
<li>Anna Becchi (contributor, former student)</li>
<li>Idriss Riouak (contributor, former student)</li>
</ul>

Besides writing/improving PHAVerLite source code,
contributions to the project include the design and implementation
of a few ad-hoc algorithms on NNC polyhedra (thereby improving PPLite)
and the development of a stand-alone tool for translating
SpaceEx models into PHAVer syntax.

<HR>

<A NAME="downloads">
<h3>Available downloads</h3>
Note: older versions are no longer maintained; we highly recommend
to switch to the most recent one.
<ul>

<li>
2024-04-12:
<a href="releases/phaverlite-0.7.tar.gz">PHAVerLite 0.7 can be downloaded</a>.
<br>
This is based on PPLite 0.12.
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.7_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 5.15.0-101.
</li>

<li>
2023-05-29:
<a href="releases/phaverlite-0.6.tar.gz">PHAVerLite 0.6 can be downloaded</a>.
<br>
This is based on PPLite 0.10.2.
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.6_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 5.15.0-72.
</li>

<li>
2022-11-03:
<a href="releases/phaverlite-0.5.tar.gz">PHAVerLite 0.5 can be downloaded</a>.
<br>
This is based on PPLite 0.8.
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.5_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 5.4.0-131-generic.
</li>
<li>
2020-11-24:
<a href="releases/phaverlite-0.4.tar.gz">PHAVerLite 0.4 can be downloaded</a>.
<br>
This is based on PPLite 0.7.
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.4_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 4.15.0-123.
</li>
<li>
2020-06-15:
<a href="releases/phaverlite-0.3.1.tar.gz">PHAVerLite 0.3.1 can be downloaded</a>.
<br>
This fixes a bug in version 0.3 (no other changes).
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.3.1_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 4.15.0-106.
</li>
<li>
2020-04-23:
<a href="releases/phaverlite-0.3.tar.gz">PHAVerLite 0.3 can be downloaded</a>.
<br>
This is based on PPLite 0.6.
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.3_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 4.15.0-96.
</li>
<li>2019-10-17:
<a href="releases/phaverlite-0.2.1.tar.gz">PHAVerLite 0.2.1 can be downloaded</a>.
<br>
This is based on PPLite 0.5.1, which fixes a bug that may be affecting
the computation of discrete transitions.
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.2.1_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 4.15.0-65.
</li>
<li>2019-07-12:
<a href="releases/phaverlite-0.2.tar.gz">PHAVerLite 0.2 can be downloaded</a>.
<br>
For convenience, we also distribute the (gzipped)
<a href="releases/phaverlite-0.2_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 4.15.0-54.
</li>
<li>2019-03-08:
<a href="releases/phaverlite-0.1.tar.gz">PHAVerLite 0.1 can be downloaded</a>.
<br>
For convenience, we also
distribute the (gzipped)
<a href="releases/phaverlite-0.1_static.gz">statically linked executable</a>
for x86_64 machines, built on Linux 4.15.0-46;
this may work on other reasonably recent Linux distributions as well.
</li>
</ul>

<hr>

<h3>PHAVerLite's specification language</h3>
PHAVerLite inherits most of
<a href="http://www-verimag.imag.fr/~frehse/phaver_web/phaver_lang.pdf">the
specification language of PHAVer</a>.

<P>
A few of the language commands and parameters of PHAVer are not supported;
also, a few parameters have been subject to renaming in an attempt
to somehow improve consistency.
Here is a brief summary of the main changes.

<ul>

<li>
PHAVer's keywords
<strong>do</strong>, <strong>end</strong>, <strong>goto</strong>,
<strong>sync</strong>, <strong>wait</strong> are treated
as <em>identifiers</em> in PHAVerLite; hence, they can be used
as location names and/or syncronization label names
without causing a syntax error.</li>

<li>
Parameter USE_CONVEX_HULL has been renamed to REACH_USE_CONVEX_HULL.
</li>
<li>Added parameter REACH_USE_CONSTRAINT_HULL.</li>

<li>
Parameter CHEAP_CONTAIN_RETURN_OTHERS has been renamed to
REACH_CHEAP_CONTAINS.
</li>
<li>Added parameter REACH_CHEAP_CONTAINS_USE_BBOX.</li>

<li>
Parameter ELAPSE_TIME has been renamed to REACH_USE_TIME_ELAPSE.
</li>

<li>
Parameter SEARCH_METHOD has been changed to only accept 3 values
(rather than the 8 values accepted by PHAVer):
  <ul>
  <li>value 0: transaction based
      (corresponding to value 0 in PHAVer);</li>
  <li>value 1: topological sort of all states
      (corresponding to value 7 in PHAVer);</li>
  <li>value 2: topological sort of reachable states
      (corresponding to value 6 in PHAVer);
      this is the value used by default.</li>
  </ul>
</li>

<li>Parameter REACH_ONLY_EXPLORE has been removed.</li>
<li>Parameter REFINE_LOCATION_PLANE has been removed.</li>
<li>All parameters related to simulation checking have been removed
    (as simulation checking is no longer supported).</li>
</ul>

