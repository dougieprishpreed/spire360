using System;
using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace Spire360Installer
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            // --snapshot <png>: the window drawn offscreen, for docs and
            // checks (nothing runs: its start-up waits for the window to show)
            if (e.Args.Length == 2 && e.Args[0] == "--snapshot")
            {
                var w = new MainWindow();
                var size = new Size(w.Width, w.Height);
                var content = (FrameworkElement)w.Content;
                w.Content = null;
                var root = new System.Windows.Controls.Border { Background = w.Background, Child = content };
                System.Windows.Documents.TextElement.SetForeground(root, w.Foreground);
                System.Windows.Documents.TextElement.SetFontFamily(root, w.FontFamily);
                root.Measure(size);
                root.Arrange(new Rect(size));
                root.UpdateLayout();
                var bmp = new RenderTargetBitmap((int)size.Width, (int)size.Height, 96, 96, PixelFormats.Pbgra32);
                bmp.Render(root);
                var png = new PngBitmapEncoder();
                png.Frames.Add(BitmapFrame.Create(bmp));
                using (var f = File.Create(e.Args[1])) png.Save(f);
                Shutdown();
                return;
            }
            new MainWindow().Show();
        }
    }
}
