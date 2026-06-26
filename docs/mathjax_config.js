// Custom MathJax v2 configuration for Doxygen HTML output
// Configures MathJax to recognize standard $$ and $ delimiters in Markdown/HTML pages.
if (typeof MathJax !== 'undefined' && MathJax.Hub) {
  MathJax.Hub.Config({
    tex2jax: {
      inlineMath: [['$','$'], ['\\(','\\)']],
      displayMath: [['$$','$$'], ['\\[','\\]']],
      processEscapes: true
    }
  });
}
