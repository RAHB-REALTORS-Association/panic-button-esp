<script>
  import Card from '../lib/Card.svelte';
  import Button from '../lib/Button.svelte';
</script>

<div class="mt-6">
  <Card>
    <div class="text-center py-8">
      <div class="text-6xl font-bold text-purple-600 mb-4">404</div>
      <h2 class="text-2xl font-semibold text-gray-700 dark:text-gray-200 mb-4">
        Page Not Found
      </h2>
      <p class="text-gray-600 dark:text-gray-400 mb-6">
        The page you're looking for doesn't seem to exist.
      </p>
      <Button color="primary" icon="fas fa-home" on:click={() => window.location.href = '#/'}>
        Go Home
      </Button>
    </div>
  </Card>
</div>