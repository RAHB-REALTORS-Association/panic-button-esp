<script>
  export let title = '';
  export let padding = true;
</script>

<div class="min-w-0 p-4 bg-white rounded-lg shadow-xs dark:bg-gray-800 mb-8">
  {#if title}
    <h4 class="mb-4 font-semibold text-gray-800 dark:text-gray-300">
      {title}
    </h4>
  {/if}
  <div class={padding ? 'p-3' : ''}>
    <slot></slot>
  </div>
</div>