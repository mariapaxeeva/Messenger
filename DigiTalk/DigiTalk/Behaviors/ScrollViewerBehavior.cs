// Поведение Scroll Viewer: автоматическая прокрутка вниз при добавлении новых сообщений в чат
using Avalonia;
using Avalonia.Controls;

namespace DigiTalk.Behaviors
{
    public static class ScrollViewerBehavior
    {
        public static readonly AttachedProperty<bool> AutoScrollProperty =
            AvaloniaProperty.RegisterAttached<Control, bool>("AutoScroll", typeof(ScrollViewerBehavior), false, false);

        static ScrollViewerBehavior()
        {
            AutoScrollProperty.Changed.AddClassHandler<ScrollViewer>((scrollViewer, args) =>
            {
                if (args.NewValue is bool autoScroll)
                {
                    if (autoScroll)
                    {
                        scrollViewer.ScrollChanged += ScrollViewer_ScrollChanged;
                        // Принудительно прокручиваю вниз при первом включении
                        scrollViewer.ScrollToEnd();
                    }
                    else
                    {
                        scrollViewer.ScrollChanged -= ScrollViewer_ScrollChanged;
                    }
                }
            });
        }

        private static void ScrollViewer_ScrollChanged(object? sender, ScrollChangedEventArgs e)
        {
            if (sender is ScrollViewer scrollViewer && e.ExtentDelta != Vector.Zero)
            {
                scrollViewer.ScrollToEnd();
            }
        }

        public static void SetAutoScroll(ScrollViewer element, bool value) =>
            element.SetValue(AutoScrollProperty, value);

        public static bool GetAutoScroll(ScrollViewer element) =>
            element.GetValue(AutoScrollProperty);
    }
}